#include "pf/base/log.h"
#include "common/setting.h"
#include "archive/node/logicmanager.h"
#include "engine/system.h"

engine::System *g_engine_system = NULL;

template <> 
engine::System *pf_base::Singleton<engine::System>::singleton_ = NULL;

namespace engine {

System *System::getsingleton_pointer() {
  return singleton_;
}

System &System::getsingleton() {
  Assert(singleton_);
  return *singleton_;
}

System::System() {
  //do nothing
}

System::~System() {
  SAFE_DELETE(g_setting);
}

pf_db::Manager *System::get_dbmanager() {
  __ENTER_FUNCTION
    pf_db::Manager *dbmanager = NULL;
    bool is_usethread = getconfig_boolvalue(ENGINE_CONFIG_DB_RUN_ASTHREAD);
    if (is_usethread) {
      dbmanager = dynamic_cast<pf_db::Manager *>(db_thread_);
    } else {
      dbmanager = db_manager_;
    }
    return dbmanager;
  __LEAVE_FUNCTION
    return NULL;
}

bool System::init() {
  __ENTER_FUNCTION
    pf_base::global::set_applicationname(_APPLICATION_NAME);
    DEBUGPRINTF("(###) engine for (%s) start...", APPLICATION_NAME);
    if (!Kernel::init_base()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME, 
                    "[engine] (System::init) base module failed");
      return false;
    }
    SLOW_LOG(ENGINE_MODULENAME, "[engine] (System::init) base module success");
    SLOW_LOG(ENGINE_MODULENAME, "[engine] (System::init) start setting module");
    if (!init_setting()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME, 
                    "[engine] (System::init) setting module failed");
      return false;
    }
    SLOW_LOG(ENGINE_MODULENAME, 
             "[engine] (System::init) setting module success");
    setconfig(ENGINE_CONFIG_DB_ISACTIVE, true);
    setconfig(
        ENGINE_CONFIG_DB_CONNECTION_OR_DBNAME, 
        SETTING_POINTER->share_memory_info_.db_connection_ordbname);
    setconfig(
        ENGINE_CONFIG_DB_USERNAME,
        SETTING_POINTER->share_memory_info_.db_user);
    setconfig(
        ENGINE_CONFIG_DB_PASSWORD,
        SETTING_POINTER->share_memory_info_.db_password);
    setconfig(
        ENGINE_CONFIG_DB_CONNECTOR_TYPE,
        SETTING_POINTER->share_memory_info_.db_connectortype);
    if (!Kernel::init()) return false;
    SLOW_LOG(ENGINE_MODULENAME, "[engine] (System::init) start archive module");
    if (!init_archive()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME, 
                    "[engine] (System::init) archive module failed");
      return false;
    }
    SLOW_LOG(ENGINE_MODULENAME, 
             "[engine] (System::init) archive module success");
    return true;
  __LEAVE_FUNCTION
    return false;
}

void System::stop() {
  __ENTER_FUNCTION
    SLOW_LOG(ENGINE_MODULENAME, "[engine] (System::stop) archive module");
    stop_archive();
    Kernel::stop();
  __LEAVE_FUNCTION
}

bool System::init_setting() {
  __ENTER_FUNCTION
    if (!SETTING_POINTER) g_setting = new common::Setting();
    if (!SETTING_POINTER) return false;
    bool result = SETTING_POINTER->init();
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_archive() {
  __ENTER_FUNCTION
    if (!ARCHIVE_NODELOGIC_MANAGER_POINTER) 
      g_archive_nodelogic_manager = new archive::node::LogicManager();
    if (!ARCHIVE_NODELOGIC_MANAGER_POINTER) return false;
    if (!ARCHIVE_NODELOGIC_MANAGER_POINTER->allocate()) return false;
    if (!ARCHIVE_NODELOGIC_MANAGER_POINTER->init()) return false;
    return true;
  __LEAVE_FUNCTION
    return false;
}

void System::stop_archive() {
  __ENTER_FUNCTION
    if (!ARCHIVE_NODELOGIC_MANAGER_POINTER) return;
    ARCHIVE_NODELOGIC_MANAGER_POINTER->release();
  __LEAVE_FUNCTION
}

bool System::loop_handle() {
  __ENTER_FUNCTION
    if (!Kernel::loop_handle()) return false; //核心优先
    if (ARCHIVE_NODELOGIC_MANAGER_POINTER)
      ARCHIVE_NODELOGIC_MANAGER_POINTER->tick();
    return true;
  __LEAVE_FUNCTION
    return false;
}

} //namespace engine
