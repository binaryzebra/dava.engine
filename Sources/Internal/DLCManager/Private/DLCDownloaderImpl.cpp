#include "DLCManager/Private/DLCDownloaderImpl.h"

#include "DLC/Downloader/CurlDownloader.h"

#include "Logger/Logger.h"
#include "FileSystem/File.h"

#include <curl/curl.h>

namespace DAVA
{
struct DefaultWriter : DLCDownloader::IWriter
{
    DefaultWriter(const String& outputFile)
    {
        f = File::Create(outputFile, File::OPEN | File::WRITE | File::CREATE);
        if (!f)
        {
            DAVA_THROW(Exception, "can't create output file: " + outputFile);
        }
    }
    ~DefaultWriter()
    {
        f->Release();
        f = nullptr;
    }

    /** save next buffer bytes into memory or file */
    uint64 Save(const void* ptr, uint64 size) override
    {
        uint32 writen = f->Write(ptr, static_cast<uint32>(size));
        return writen;
    }
    /** return current size of saved byte stream */
    uint64 GetSeekPos() override
    {
        return f->GetPos();
    }

    void Truncate() override
    {
        f->Truncate(0);
    }

    uint64 SpaceLeft() override
    {
        return std::numeric_limits<uint64>::max();
    }

private:
    File* f = nullptr;
};

// ONLY use this global variable from DLCDownloader thread
static UnorderedMap<CURL*, DLCDownloader::Task*> taskMap;
Mutex mutexTaskMap;

DLCDownloaderImpl::DLCDownloaderImpl()
{
    if (!CurlDownloader::isCURLInit && CURLE_OK != curl_global_init(CURL_GLOBAL_ALL))
    {
        DAVA_THROW(Exception, "curl_global_init fail");
    }

    multiHandle = curl_multi_init();

    if (multiHandle == nullptr)
    {
        DAVA_THROW(Exception, "curl_multi_init fail");
    }

    downloadThread = Thread::Create([this] { DownloadThreadFunc(); });
    downloadThread->SetName("DLCDownloader");
    downloadThread->Start();
}

DLCDownloaderImpl::~DLCDownloaderImpl()
{
    if (downloadThread != nullptr)
    {
        if (downloadThread->IsJoinable())
        {
            downloadThread->Cancel();
            downloadSem.Post(100); // just to resume if waiting
            downloadThread->Join();
        }
    }

    curl_multi_cleanup(multiHandle);
    multiHandle = nullptr;

    for (CURL* easy : easyHandlesAll)
    {
        curl_easy_cleanup(easy);
    }
    easyHandlesAll.clear();

    if (!CurlDownloader::isCURLInit)
    {
        curl_global_cleanup();
    }
}

DLCDownloader::Task* DLCDownloaderImpl::StartTask(const String& srcUrl,
                                                  const String& dstPath,
                                                  TaskType taskType,
                                                  IWriter* dstWriter,
                                                  uint64 rangeOffset,
                                                  uint64 rangeSize,
                                                  int16 partsCount,
                                                  int32 timeout,
                                                  int32 retriesCount)
{
    IWriter* defaultWriter = nullptr;
    if (dstWriter == nullptr && TaskType::SIZE != taskType)
    {
        try
        {
            defaultWriter = new DefaultWriter(dstPath);
        }
        catch (Exception& ex)
        {
            Logger::Error("%s", ex.what());
            return nullptr;
        }
    }

    Task* task = new Task();

    auto& info = task->info;

    info.retriesCount = retriesCount;
    info.rangeOffset = rangeOffset;
    info.rangeSize = rangeSize;
    info.partsCount = (partsCount == -1) ? 1 : partsCount;
    info.srcUrl = srcUrl;
    info.dstPath = dstPath;
    info.timeoutSec = timeout;
    info.type = taskType;
    info.customWriter = dstWriter;

    auto& state = task->status;
    state.state = TaskState::JustAdded;
    state.error = TaskError();
    state.fileErrno = 0;
    state.retriesLeft = info.retriesCount;
    state.sizeDownloaded = 0;
    state.sizeTotal = info.rangeSize;

    if (defaultWriter != nullptr && TaskType::SIZE != taskType)
    {
        task->defaultWriter.reset(defaultWriter);
    }

    {
        LockGuard<Mutex> lock(mutexTaskQueue);
        taskQueue.push_back(task);
    }

    ++numOfNewTasks;

    downloadSem.Post(1);

    return task;
}

void DLCDownloaderImpl::RemoveTask(Task* task)
{
    LockGuard<Mutex> lock(mutexTaskQueue);

    if (task->status.state == TaskState::Downloading)
    {
        for (CURL* easy : task->easyHandles)
        {
            {
                LockGuard<Mutex> l(mutexTaskMap);
                taskMap.erase(easy);
            }

            CURLMcode r = curl_multi_remove_handle(multiHandle, easy);
            DVASSERT(CURLM_OK == r);

            curl_easy_cleanup(easy);
        }
        task->easyHandles.clear();
    }
    auto it = std::find(begin(taskQueue), end(taskQueue), task);
    if (it != end(taskQueue))
    {
        taskQueue.erase(it);
    }

    delete task;
}

void DLCDownloaderImpl::WaitTask(Task* task)
{
    while (true)
    {
        TaskStatus status = GetTaskStatus(task);
        if (status.state == TaskState::Finished)
        {
            break;
        }
        Thread::Sleep(50); // TODO reimplement it!
    }
}

const DLCDownloader::TaskInfo* DLCDownloaderImpl::GetTaskInfo(Task* task)
{
    return &task->info;
}

DLCDownloader::TaskStatus DLCDownloaderImpl::GetTaskStatus(Task* task)
{
    TaskStatus status;
    {
        LockGuard<Mutex> lock(mutexTaskQueue);
        status = task->status;
    }

    return status;
}

static DLCDownloader::Task* FindJustEddedTask(Deque<DLCDownloader::Task*>& taskQueue)
{
    for (auto task : taskQueue)
    {
        if (task->status.state == DLCDownloader::TaskState::JustAdded)
        {
            return task;
        }
    }
    return nullptr;
}

static CURL* CurlSimpleInitHandle()
{
    CURL* curl_handle = curl_easy_init();
    DVASSERT(curl_handle != nullptr);

    CURLcode code = CURLE_OK;
    code = curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L); // Do we need it?
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11");
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    DVASSERT(CURLE_OK == code);
    return curl_handle;
}

static void CurlSetTimeout(DLCDownloader::Task* justAddedTask, CURL* easyHandle)
{
    CURLcode code = CURLE_OK;

    long operationTimeout = static_cast<long>(justAddedTask->info.timeoutSec);

    code = curl_easy_setopt(easyHandle, CURLOPT_CONNECTTIMEOUT, operationTimeout);
    DVASSERT(CURLE_OK == code);

    // we could set operation time limit which produce timeout if operation takes time.
    code = curl_easy_setopt(easyHandle, CURLOPT_TIMEOUT, 0L);
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(easyHandle, CURLOPT_DNS_CACHE_TIMEOUT, operationTimeout);
    DVASSERT(CURLE_OK == code);

    code = curl_easy_setopt(easyHandle, CURLOPT_SERVER_RESPONSE_TIMEOUT, operationTimeout);
    DVASSERT(CURLE_OK == code);
}

static size_t CurlDataRecvHandler(void* ptr, size_t size, size_t nmemb, void* part)
{
    DLCDownloader::IWriter* writer = static_cast<DLCDownloader::IWriter*>(part);
    DVASSERT(writer != nullptr);

    size_t fullSizeToWrite = size * nmemb;

    uint64 writen = 0;

    size_t spaceLeft = static_cast<size_t>(writer->SpaceLeft());
    if (spaceLeft >= fullSizeToWrite)
    {
        writen = writer->Save(ptr, fullSizeToWrite);
    }
    else
    {
        writen = writer->Save(ptr, spaceLeft);
    }

    return static_cast<size_t>(writen);
}

static void StoreHandle(DLCDownloader::Task* justAddedTask, CURL* easyHandle)
{
    justAddedTask->easyHandles.emplace(easyHandle);
    {
        LockGuard<Mutex> l(mutexTaskMap);
        taskMap.emplace(easyHandle, justAddedTask);
    }
}

static void SetupFullDownload(DLCDownloader::Task* justAddedTask)
{
    CURLcode code = CURLE_OK;

    CURL* easyHandle = CurlSimpleInitHandle();
    DVASSERT(easyHandle != nullptr);

    StoreHandle(justAddedTask, easyHandle);

    auto& info = justAddedTask->info;

    DLCDownloader::IWriter* writer = info.customWriter != nullptr ? info.customWriter : justAddedTask->defaultWriter.get();
    DVASSERT(writer != nullptr);

    const char* url = justAddedTask->info.srcUrl.c_str();
    DVASSERT(url != nullptr);

    code = curl_easy_setopt(easyHandle, CURLOPT_URL, url);
    DVASSERT(code == CURLE_OK);

    code = curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, CurlDataRecvHandler);
    DVASSERT(code == CURLE_OK);

    code = curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, writer);
    DVASSERT(code == CURLE_OK);

    bool hasRangeStart = info.rangeOffset != 0;
    bool hasRangeFinish = info.rangeOffset != 0;

    if (hasRangeStart || hasRangeFinish)
    {
        StringStream ss;
        if (hasRangeStart)
        {
            ss << info.rangeOffset;
        }
        ss << '-';
        if (hasRangeFinish)
        {
            ss << info.rangeSize;
        }
        String s = ss.str();
        code = curl_easy_setopt(easyHandle, CURLOPT_RANGE, s.c_str());
        DVASSERT(code == CURLE_OK);
    }

    // set all timeouts
    CurlSetTimeout(justAddedTask, easyHandle);
}

static void SetupResumeDownload(DLCDownloader::Task* justAddedTask)
{
    CURLcode code = CURLE_OK;

    CURL* easyHandle = CurlSimpleInitHandle();
    DVASSERT(easyHandle != nullptr);

    StoreHandle(justAddedTask, easyHandle);

    auto& info = justAddedTask->info;

    DLCDownloader::IWriter* writer = info.customWriter != nullptr ? info.customWriter : justAddedTask->defaultWriter.get();
    DVASSERT(writer != nullptr);

    const char* url = justAddedTask->info.srcUrl.c_str();
    DVASSERT(url != nullptr);

    code = curl_easy_setopt(easyHandle, CURLOPT_URL, url);
    DVASSERT(code == CURLE_OK);

    code = curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, CurlDataRecvHandler);
    DVASSERT(code == CURLE_OK);

    code = curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, writer);
    DVASSERT(code == CURLE_OK);

    {
        char8 rangeStr[32];
        sprintf(rangeStr, "%lld-", writer->GetSeekPos());
        code = curl_easy_setopt(easyHandle, CURLOPT_RANGE, rangeStr);
        DVASSERT(code == CURLE_OK);
    }

    // set all timeouts
    CurlSetTimeout(justAddedTask, easyHandle);
}

static void SetupGetSizeDownload(DLCDownloader::Task* justAddedTask)
{
    CURLcode code = CURLE_OK;

    CURL* easyHandle = CurlSimpleInitHandle();
    DVASSERT(easyHandle != nullptr);

    StoreHandle(justAddedTask, easyHandle);

    code = curl_easy_setopt(easyHandle, CURLOPT_HEADER, 0L);
    DVASSERT(CURLE_OK == code);

    const char* url = justAddedTask->info.srcUrl.c_str();
    DVASSERT(url != nullptr);

    code = curl_easy_setopt(easyHandle, CURLOPT_URL, url);
    DVASSERT(CURLE_OK == code);

    // Don't return the header (we'll use curl_getinfo();
    code = curl_easy_setopt(easyHandle, CURLOPT_NOBODY, 1L);
    DVASSERT(CURLE_OK == code);

    //CURLcode curlStatus = curl_easy_perform(easyHandle);
    //double sizeToDownload = 0.0;
    //curl_easy_getinfo(easyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &sizeToDownload);
    //Logger::Info("size is = %f", sizeToDownload);
}

// return number of still running handles
static size_t CurlPerform(CURLM* multiHandle);

bool DLCDownloaderImpl::TakeOneNewTaskFromQueue()
{
    Task* justAddedTask = nullptr;

    {
        LockGuard<Mutex> lock(mutexTaskQueue);
        justAddedTask = FindJustEddedTask(taskQueue);
    }

    if (justAddedTask != nullptr)
    {
        switch (justAddedTask->info.type)
        {
        case TaskType::FULL:
            SetupFullDownload(justAddedTask);
            break;
        case TaskType::RESUME:
            SetupResumeDownload(justAddedTask);
            break;
        case TaskType::SIZE:
            SetupGetSizeDownload(justAddedTask);
            break;
        }

        for (CURL* easyHandle : justAddedTask->easyHandles)
        {
            CURLMcode code = curl_multi_add_handle(multiHandle, easyHandle);
            DVASSERT(CURLM_OK == code);
        }
        --numOfNewTasks;
    }
    return justAddedTask != nullptr;
}

void DLCDownloaderImpl::DownloadThreadFunc()
{
    const size_t maxSimultaniousDownloads = 1024;
    Thread* currentThread = Thread::Current();
    DVASSERT(currentThread != nullptr);

    bool stillRuning = true;
    size_t numOfRunningHandles = 0;

    while (!currentThread->IsCancelling())
    {
        if (!stillRuning)
        {
            downloadSem.Wait();
            stillRuning = true;
        }

        size_t num = maxSimultaniousDownloads - numOfRunningHandles;
        for (; num > 0; --num)
        {
            if (!TakeOneNewTaskFromQueue())
            {
                break; // no more new tasks
            }
        }

        while (stillRuning)
        {
            numOfRunningHandles = CurlPerform(multiHandle);

            if (numOfRunningHandles == 0)
            {
                stillRuning = false;
            }

            CURLMsg* curlMsg = nullptr;

            /* call curl_multi_perform or curl_multi_socket_action first, then loop
			through and check if there are any transfers that have completed */
            do
            {
                int msgq = 0;
                curlMsg = curl_multi_info_read(multiHandle, &msgq);
                if (curlMsg && (curlMsg->msg == CURLMSG_DONE))
                {
                    CURL* easyHandle = curlMsg->easy_handle;
                    Task* finishedTask = nullptr;

                    {
                        LockGuard<Mutex> l(mutexTaskMap);
                        auto it = taskMap.find(easyHandle);
                        DVASSERT(it != end(taskMap));

                        finishedTask = it->second;

                        taskMap.erase(it);
                    }

                    if (finishedTask->info.type == TaskType::SIZE)
                    {
                        float64 sizeToDownload = 0.0; // curl need double! do not change https://curl.haxx.se/libcurl/c/CURLINFO_CONTENT_LENGTH_DOWNLOAD.html
                        CURLcode code = curl_easy_getinfo(easyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &sizeToDownload);
                        DVASSERT(CURLE_OK == code);
                        finishedTask->status.sizeTotal = static_cast<uint64>(sizeToDownload);
                    }

                    finishedTask->status.error.curlErr = curlMsg->data.result;

                    finishedTask->easyHandles.erase(easyHandle);
                    if (finishedTask->easyHandles.empty())
                    {
                        finishedTask->defaultWriter.reset();
                        finishedTask->status.state = TaskState::Finished;
                    }

                    curl_multi_remove_handle(multiHandle, easyHandle);
                    curl_easy_cleanup(easyHandle);
                }
            } while (curlMsg);

            if (stillRuning)
            {
                if (numOfRunningHandles < maxSimultaniousDownloads)
                {
                    if (numOfNewTasks > 0)
                    {
                        break; // get more new task to do it simultaneously
                    }
                }
            }
        }
    } // !currentThread->IsCancelling()
}

// from https://curl.haxx.se/libcurl/c/curl_multi_perform.html
// from https://curl.haxx.se/libcurl/c/curl_multi_wait.html
static size_t CurlPerform(CURLM* multiHandle)
{
    int stillRunning = 0;
    //long curl_timeo;

    CURLMcode mc;
    int numfds;

    mc = curl_multi_perform(multiHandle, &stillRunning);
    DVASSERT(mc == CURLM_OK);

    if (mc == CURLM_OK)
    {
        /* wait for activity, timeout or "nothing" */
        mc = curl_multi_wait(multiHandle, nullptr, 0, 1000, &numfds);
    }

    if (mc != CURLM_OK)
    {
        Logger::Error("curl_multi failed, code %d.n", mc);
        return 0;
    }

    /* if there are still transfers, loop! */
    return static_cast<size_t>(stillRunning);
}

} // end namespace DAVA
