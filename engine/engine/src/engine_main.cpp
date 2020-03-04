#include <app/app.h>
#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/dns.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <graphics/graphics.h>
#include <crash/crash.h>

#include "engine.h"
#include "engine_version.h"

static void AppCreate(void* _ctx)
{
    (void)_ctx;
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "engine_main");

#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmHashEnableReverseHash(dLib::IsDebugMode());

    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmDNS::Initialize();
    dmMemProfile::Initialize();
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmLogParams params;
    dmLogInitialize(&params);
}

static void AppDestroy(void* _ctx)
{
    (void)_ctx;
    dmGraphics::Finalize();
    dmLogFinalize();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    dmDNS::Finalize();
    dmSocket::Finalize();
}

int engine_main(int argc, char *argv[])
{
    dmApp::Params params;
    params.m_Argc = argc;
    params.m_Argv = argv;
    params.m_AppCtx = 0;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = (dmApp::EngineCreate)dmEngineCreate;
    params.m_EngineDestroy = (dmApp::EngineDestroy)dmEngineDestroy;
    params.m_EngineUpdate = (dmApp::EngineUpdate)dmEngineUpdate;
    params.m_EngineGetResult = (dmApp::EngineGetResult)dmEngineGetResult;
    return dmApp::Run(&params);
}
