#include "pf/base/log.h"
#include "pf/net/packet/factorymanager.h"
#include "common/net/packetfactory.h"
#include "common/setting.h"
#include "connection/login.h"
#include "connection/counter/center.h"
#include "connection/queue/center.h"
#include "connection/queue/turn.h"
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
  __ENTER_FUNCTION
    server_netmanager_ = NULL;
    incoming_netmanager_ = NULL;
    login_netmanager_ = NULL;
  __LEAVE_FUNCTION
}

System::~System() {
  SAFE_DELETE(login_netmanager_);
  SAFE_DELETE(g_connection_queue_turn);
  SAFE_DELETE(g_connection_queue_center);
  SAFE_DELETE(g_connection_counter_center);
  SAFE_DELETE(incoming_netmanager_);
  SAFE_DELETE(server_netmanager_);
  SAFE_DELETE(g_packetfactory_manager);
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

void System::set_netmanager(pf_net::Manager *netmanager) {
  __ENTER_FUNCTION
    bool is_usethread = getconfig_boolvalue(ENGINE_CONFIG_NET_RUN_ASTHREAD); 
    if (is_usethread) {
      net_thread_ = dynamic_cast<pf_engine::thread::Net *>(netmanager);
    } else {
      net_manager_ = netmanager;
    }
  __LEAVE_FUNCTION
}

pf_net::Manager *System::get_netmanager() {
  __ENTER_FUNCTION
    pf_net::Manager *netmanager = NULL;
    bool is_usethread = getconfig_boolvalue(ENGINE_CONFIG_NET_RUN_ASTHREAD);
    if (is_usethread) {
      netmanager = dynamic_cast<pf_net::Manager *>(net_thread_);
    } else {
      netmanager = net_manager_;
    }
    return netmanager;
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

    setconfig(ENGINE_CONFIG_PERFORMANCE_ISACTIVE, true);
    setconfig(ENGINE_CONFIG_DB_ISACTIVE, true);
    setconfig(ENGINE_CONFIG_NET_ISACTIVE, true); //一定要在初始化各个系统前设置
    setconfig(ENGINE_CONFIG_NET_RUN_ASTHREAD, true);
    setconfig(
        ENGINE_CONFIG_DB_CONNECTION_OR_DBNAME, 
        SETTING_POINTER->login_info_.db_connection_ordbname);
    setconfig(
        ENGINE_CONFIG_DB_USERNAME,
        SETTING_POINTER->login_info_.db_user);
    setconfig(
        ENGINE_CONFIG_DB_PASSWORD,
        SETTING_POINTER->login_info_.db_password);
    setconfig(
        ENGINE_CONFIG_DB_CONNECTOR_TYPE,
        SETTING_POINTER->login_info_.db_connectortype);
    SLOW_LOG(ENGINE_MODULENAME, 
             "[engine] (System::init) setting module success");
    if (!NET_PACKET_FACTORYMANAGER_POINTER)
      g_packetfactory_manager = new pf_net::packet::FactoryManager();
    if (!NET_PACKET_FACTORYMANAGER_POINTER) return false;
    NET_PACKET_FACTORYMANAGER_POINTER->set_function_registerfactories(
        common::net::registerfactories);
    NET_PACKET_FACTORYMANAGER_POINTER->set_function_isvalid_packetid(
        common::net::isvalid_packetid);
    uint16_t factorysize = common::net::get_facctorysize();
    NET_PACKET_FACTORYMANAGER_POINTER->setsize(factorysize);
    bool result = Kernel::init();
    NET_PACKET_FACTORYMANAGER_POINTER->init();
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_net() {
  __ENTER_FUNCTION
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::init_net) server start");
    if (!init_net_server()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME,
                    "[engine] (System::init_net) server error");
      return false;
    }
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::init_net) incoming start");
    if (!init_net_incoming()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME,
                    "[engine] (System::init_net) incoming error");
      return false;
    }
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::init_net) login start");
    if (!init_net_login()) {
      SLOW_ERRORLOG(ENGINE_MODULENAME,
                    "[engine] (System::init_net) login error");
      return false;
    }
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_setting() {
  __ENTER_FUNCTION
    if (!SETTING_POINTER)
      g_setting = new common::Setting();
    if (!SETTING_POINTER) return false;
    bool result = SETTING_POINTER->init();
    return result;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_net_connectionpool() {
  __ENTER_FUNCTION
    if (!get_netmanager()) return false; //网络管理器还未初始化
    uint16_t connectionmax = static_cast<uint16_t>(
        getconfig_int32value(ENGINE_CONFIG_NET_CONNECTION_MAX));
    get_netmanager()->getpool()->init(connectionmax);
    uint16_t i = 0;
    for (i = 0; i < connectionmax; ++i) {
      connection::Login *loginconnection = new connection::Login();
      Assert(loginconnection);
      get_netmanager()->getpool()->init_data(i, loginconnection);
    }
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_net_incoming() { //接收管理器将作为主管理器
  __ENTER_FUNCTION
    setconfig(ENGINE_CONFIG_NET_CONNECTION_MAX,
              SETTING_POINTER->login_info_.net_connectionmax);
    incoming_netmanager_ = new thread::net::Incoming();
    setconfig(ENGINE_CONFIG_NET_LISTEN_IP, 
      CONNECTION_MANAGER_SERVER_POINTER->get_current_serverinfo()->ip);
    setconfig(ENGINE_CONFIG_NET_LISTEN_PORT,
      CONNECTION_MANAGER_SERVER_POINTER->get_current_serverinfo()->port);
    if (NULL == incoming_netmanager_) return false;
    pf_net::Manager *netmanager = 
      dynamic_cast<pf_net::Manager *>(incoming_netmanager_);
    set_netmanager(netmanager);
    
    if (!CONNECTION_COUNTER_CENTER_POINTER) //中心计数器
      g_connection_counter_center = new connection::counter::Center();
    if (!CONNECTION_COUNTER_CENTER_POINTER) return false;

    if (!CONNECTION_QUEUE_CENTER_POINTER) //中心排队
      g_connection_queue_center = new connection::queue::Center();
    if (!CONNECTION_QUEUE_CENTER_POINTER) return false;

    if (!CONNECTION_QUEUE_TURN_POINTER) //登陆排队信息
      g_connection_queue_turn = new connection::queue::Turn();
    if (!CONNECTION_QUEUE_TURN_POINTER) return false;

    if (!Kernel::init_net()) return false;
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_net_login() {
  __ENTER_FUNCTION
    login_netmanager_ = new thread::net::Login();
    if (NULL == login_netmanager_) return false;
    if (!login_netmanager_->init()) return false;
    return true;
  __LEAVE_FUNCTION
    return false;
}

bool System::init_net_server() {
  __ENTER_FUNCTION
    server_netmanager_ = new thread::net::Server();
    if (NULL == server_netmanager_) return false;
    if (!server_netmanager_->init()) return false;
    return true;
  __LEAVE_FUNCTION
    return false;
}

void System::run_net() {
  __ENTER_FUNCTION
    run_net_server();
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::run_net) server");
    run_net_incoming();
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::run_net) incoming");
    run_net_login();
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::run_net) login");
  __LEAVE_FUNCTION
}

void System::stop_net() {
  __ENTER_FUNCTION
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::stop_net) server");
    stop_net_server();
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::stop_net) incoming");
    stop_net_incoming();
    SLOW_LOG(NET_MODULENAME, 
             "[engine] (System::stop_net) login");
    stop_net_login();
  __LEAVE_FUNCTION
}

void System::run_net_incoming() {
  __ENTER_FUNCTION
    incoming_netmanager_->start();
  __LEAVE_FUNCTION
}

void System::run_net_login() {
  __ENTER_FUNCTION
    login_netmanager_->start();
  __LEAVE_FUNCTION
}
   
void System::run_net_server() {
  __ENTER_FUNCTION
    server_netmanager_->start();
  __LEAVE_FUNCTION
}

void System::stop_net_incoming() {
  __ENTER_FUNCTION
    incoming_netmanager_->stop();
  __LEAVE_FUNCTION
}

void System::stop_net_login() {
  __ENTER_FUNCTION
    login_netmanager_->stop();
  __LEAVE_FUNCTION
}

void System::stop_net_server() {
  __ENTER_FUNCTION
    server_netmanager_->stop();
  __LEAVE_FUNCTION
}

} //namespace engine
