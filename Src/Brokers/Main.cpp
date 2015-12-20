
#include <nstd/Console.h>
#include <nstd/Thread.h>
#include <nstd/Log.h>
#include <nstd/Map.h>
#include <megucoprotocol.h>

#include "Main.h"

#ifdef BROKER_BITSTAMPBTCUSD
#include "Brokers/BitstampBtcUsd.h"
typedef BitstampBtcUsd BrokerImpl;
#endif

int_t main(int_t argc, char_t* argv[])
{
  if(argc < 3)
  {
    Console::errorf("error: Missing broker attributes\n");
    return -1;
  }
  String userName(argv[1], String::length(argv[1]));
  uint64_t brokerId = String::toUInt64(argv[2]);

  Log::setFormat("%P> %m");

  //for(;;)
  //{
  //  bool stop = true;
  //  if(!stop)
  //    break;
  //}

  // create connection to bot server
  Main main;
  for(;; Thread::sleep(10 * 1000))
  {
    if(!main.connect2(userName, brokerId))
    {
      Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)main.getErrorString());
      continue;
    }
    Log::infof("Connected to zlimdb server.");

    // wait for requests
    main.process();
    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)main.getErrorString());
  }
}

Main::~Main()
{
  delete broker;
}

bool_t Main::connect2(const String& userName, uint64_t brokerId)
{
  delete broker;
  broker = 0;
  orders2.clear();
  transactions2.clear();
  balance.entity.id = 0;

  if(!connection.connect(*this))
    return false;

  // get user name and broker id
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  String brokerIdStr = String::fromUInt64(brokerId);

  // get and subscribe to user broker table
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerIdStr + "/broker", userBrokerTableId))
    return false;
  if(!connection.subscribe(userBrokerTableId, zlimdb_subscribe_flag_responder))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_entity* userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity));
        userBrokerEntity;
        userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity), &userBrokerEntity->entity))
    {
      if(userBrokerEntity->entity.id != 1)
        continue;
      String userName, key, secret;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity), userBrokerEntity->user_name_size, userName))
        return false;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity) + userName.length() + 1, userBrokerEntity->key_size, key))
        return false;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity) + userName.length() + 1 + key.length() + 1, userBrokerEntity->secret_size, secret))
        return false;
      broker = new BrokerImpl(userName, key, secret);
    }
  }
  if(connection.getErrno() != 0)
    return false;

  // subscribe to orders table
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerIdStr + "/orders", userBrokerOrdersTableId))
    return false;
  if(!connection.subscribe(userBrokerOrdersTableId, zlimdb_subscribe_flag_responder))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_order_entity* order = (const meguco_user_broker_order_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_order_entity)); 
        order;
        order = (const meguco_user_broker_order_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_order_entity), &order->entity))
      orders2.append(order->entity.id, *order);
  }
  if(connection.getErrno() != 0)
    return false;

  // get transaction table
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerIdStr + "/transactions", userBrokerTransactionsTableId))
    return false;
  if(!connection.query(userBrokerTransactionsTableId))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_transaction_entity* transaction = (const meguco_user_broker_transaction_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_transaction_entity));
        transaction;
        transaction = (const meguco_user_broker_transaction_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_transaction_entity), &transaction->entity))
      transactions2.append(transaction->entity.id, *transaction);
  }
  if(connection.getErrno() != 0)
    return false;

  // get balance table id
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerIdStr + "/balance", userBrokerBalanceTableId))
    return false;

  // get log table id
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerIdStr + "/log", userBrokerLogTableId))
    return false;

  this->userBrokerTableId = userBrokerTableId;
  return true;
}
/*
void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userBrokerOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_broker_order_entity))
      return addedUserBrokerOrder(*(meguco_user_broker_order_entity*)&entity);
  }
}
*/
/*
void_t Main::updatedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userBrokerOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_broker_order_entity))
      return updatedUserBrokerOrder(*(meguco_user_broker_order_entity*)&entity);
  }
}
*/
/*
void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == userBrokerOrdersTableId)
    return removedUserBrokerOrder(entityId);
}
*/
void_t Main::controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(tableId == userBrokerTableId)
    return controlUserBroker(requestId, entityId, controlCode, data, size);
  else if(tableId == userBrokerOrdersTableId)
    return controlUserBrokerOrder(requestId, entityId, controlCode);
  else
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
}
/*
void_t Main::addedUserBrokerOrder(const meguco_user_broker_order_entity& newOrder)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(newOrder.entity.id);
  if(it != orders2.end())
  {
    meguco_user_broker_order_entity& order = *it;
    if(Memory::compare(&order, &newOrder, sizeof(order)) == 0)
      return;
    connection.update(userBrokerOrdersTableId, order.entity);
    return;
  }

  if(newOrder.state == meguco_user_broker_order_submitting)
  {
    meguco_user_broker_order_entity order;
    if(!broker->createOrder(newOrder.entity.id, (meguco_user_broker_order_type)newOrder.type, newOrder.price, newOrder.amount, newOrder.total, order))
    {
      addLogMessage(meguco_log_error, broker->getLastError());
      order = newOrder;
      order.state = meguco_user_broker_order_error;
    }
    else
      order.state = meguco_user_broker_order_open;
    order.timeout = newOrder.timeout;
    connection.update(userBrokerOrdersTableId, order.entity);
    orders2.append(order.entity.id, order);
  }
  else
    orders2.append(newOrder.entity.id, newOrder);
}
*/
/*
void_t Main::updatedUserBrokerOrder(const meguco_user_broker_order_entity& updatedOrder)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(updatedOrder.entity.id);
  if(it == orders2.end())
    return;
  meguco_user_broker_order_entity& order = *it;
  if(Memory::compare(&order, &updatedOrder, sizeof(order)) == 0)
    return;

  if(updatedOrder.state == meguco_user_broker_order_open)
  {
    // step #1 cancel current order
    if(!broker->cancelOrder(updatedOrder.entity.id))
      return addLogMessage(meguco_log_error, broker->getLastError());
    
    // step #2 create new order with same id
    meguco_user_broker_order_entity order;
    if(!broker->createOrder(updatedOrder.entity.id, (meguco_user_broker_order_type)updatedOrder.type, updatedOrder.price, updatedOrder.amount, updatedOrder.total, order))
    {
      addLogMessage(meguco_log_error, broker->getLastError());
      order = updatedOrder;
      order.state = meguco_user_broker_order_error;
    }
    else
      order.state = meguco_user_broker_order_open;
    order.timeout = updatedOrder.timeout;
    connection.update(userBrokerOrdersTableId, order.entity);
    orders2.append(updatedOrder.entity.id, updatedOrder);
  }
  else
    orders2.append(updatedOrder.entity.id, updatedOrder);
}
*/
/*
void_t Main::removedUserBrokerOrder(uint64_t entityId)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(entityId);
  if(it == orders2.end())
    return;
  orders2.remove(entityId);
  if(!broker->cancelOrder(entityId))
  {
    meguco_user_broker_order_entity order = *it;
    order.entity.id = 0;
    order.entity.time = 0;
    uint64_t newId;
    connection.add(userBrokerOrdersTableId, order.entity, newId);
  }
}
*/
void_t Main::controlUserBroker(uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(entityId != 1)
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);

  switch(controlCode)
  {
  case meguco_user_broker_control_refresh_orders:
    {
      List<meguco_user_broker_order_entity> newOrders;
      if(!broker->loadOrders(newOrders))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
      Map<int64_t, meguco_user_broker_order_entity*> sortedNewOrders;
      for(List<meguco_user_broker_order_entity>::Iterator i = newOrders.begin(), end = newOrders.end(); i != end; ++i)
        sortedNewOrders.insert((*i).entity.time, &*i);
      HashMap<uint64_t, meguco_user_broker_order_entity> ordersMapByRaw;
      for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = orders2.begin(), end = orders2.end(); i != end; ++i)
      {
        meguco_user_broker_order_entity& order = *i;
        ordersMapByRaw.append(order.raw_id, order);
      }
      orders2.clear();
      for(Map<int64_t, meguco_user_broker_order_entity*>::Iterator i = sortedNewOrders.begin(), end = sortedNewOrders.end(); i != end; ++i)
      {
        meguco_user_broker_order_entity& order = **i;
        HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = ordersMapByRaw.find(order.raw_id);
        if(it == ordersMapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userBrokerOrdersTableId, order.entity, id))
          {
            order.entity.id = id;
            orders2.append(order.entity.id, order);
          }
        }
        else
        { // update
          order.entity.id = it->entity.id;
          if(Memory::compare(&*it, &order, sizeof(order)) != 0 &&
             connection.update(userBrokerOrdersTableId, order.entity))
            orders2.append(order.entity.id, order);
          else
            orders2.append(order.entity.id, *it);
          ordersMapByRaw.remove(it);
        }
      }
      for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = ordersMapByRaw.begin(), end = ordersMapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_broker_order_entity& order = *i;
        if(!connection.remove(userBrokerOrdersTableId, order.entity.id))
          orders2.append(order.entity.id, order);
      }
    }
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  case meguco_user_broker_control_refresh_transactions:
    {
      List<meguco_user_broker_transaction_entity> newTransactions;
      if(!broker->loadTransactions(newTransactions))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
      Map<int64_t, meguco_user_broker_transaction_entity*> sortedNewTransactions;
      for(List<meguco_user_broker_transaction_entity>::Iterator i = newTransactions.begin(), end = newTransactions.end(); i != end; ++i)
        sortedNewTransactions.insert((*i).entity.time, &*i);
      HashMap<uint64_t, meguco_user_broker_transaction_entity> transactionsapByRaw;
      for(HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator i = transactions2.begin(), end = transactions2.end(); i != end; ++i)
      {
        meguco_user_broker_transaction_entity& transaction = *i;
        transactionsapByRaw.append(transaction.raw_id, transaction);
      }
      transactions2.clear();
      for(Map<int64_t, meguco_user_broker_transaction_entity*>::Iterator i = sortedNewTransactions.begin(), end = sortedNewTransactions.end(); i != end; ++i)
      {
        meguco_user_broker_transaction_entity& transaction = **i;
        HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator it = transactionsapByRaw.find(transaction.raw_id);
        if(it == transactionsapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userBrokerTransactionsTableId, transaction.entity, id))
          {
            transaction.entity.id = id;
            transactions2.append(transaction.entity.id, transaction);
          }
        }
        else
        { // update
          transaction.entity.id = it->entity.id;
          if(Memory::compare(&*it, &transaction, sizeof(transaction)) != 0 &&
             connection.update(userBrokerTransactionsTableId, transaction.entity))
            transactions2.append(transaction.entity.id, transaction);
          else
            transactions2.append(transaction.entity.id, *it);
          transactionsapByRaw.remove(it);
        }
      }
      for(HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator i = transactionsapByRaw.begin(), end = transactionsapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_broker_transaction_entity& transaction = *i;
        if(!connection.remove(userBrokerTransactionsTableId, transaction.entity.id))
          transactions2.append(transaction.entity.id, transaction);
      }
    }
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  case meguco_user_broker_control_refresh_balance:
    {
      meguco_user_broker_balance_entity newBalance;
      if(!broker->loadBalance(newBalance))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
      if(this->balance.entity.id == 0)
      {
        uint64_t id;
        if(connection.add(userBrokerBalanceTableId, newBalance.entity, id))
          newBalance.entity.id = id;
      }
      else
      {
        newBalance.entity.id = this->balance.entity.id;
        connection.update(userBrokerBalanceTableId, newBalance.entity);
      }
      this->balance = newBalance;
    }
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  case meguco_user_broker_control_create_order:
    if(size < sizeof(meguco_user_broker_order_entity))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      const meguco_user_broker_order_entity& newOrder = *(const meguco_user_broker_order_entity*)data;
      meguco_user_broker_order_entity order;
      if(!broker->createOrder(0, (meguco_user_broker_order_type)newOrder.type, newOrder.price, newOrder.amount, newOrder.total, order))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }

      order.state = meguco_user_broker_order_open;
      order.timeout = newOrder.timeout;
      ZlimdbConnection::setEntityHeader(order.entity, 0, 0, sizeof(meguco_user_broker_order_entity));

      // add entity to user brokers table
      uint64_t id;
      if(!connection.add(userBrokerOrdersTableId, order.entity, id))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // return entity id
      return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&id, sizeof(uint64_t));
    }
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}

void_t Main::controlUserBrokerOrder(uint32_t requestId, uint64_t entityId, uint32_t controlCode)
{
  switch(controlCode)
  {
  case meguco_user_broker_order_control_cancel:
    // todo
    break;
  case meguco_user_broker_order_control_update:
    // todo
    break;
  case meguco_user_broker_order_control_remove:
    // todo
    break;
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}

void_t Main::addLogMessage(meguco_log_type type, const String& message)
{
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  meguco_log_entity* logEntity = (meguco_log_entity*)buffer;
  ZlimdbConnection::setEntityHeader(logEntity->entity, 0, 0, sizeof(meguco_log_entity));
  logEntity->type = type;
  if(!ZlimdbConnection::copyString(logEntity->entity, logEntity->message_size, message, ZLIMDB_MAX_MESSAGE_SIZE))
    return;
  uint64_t id;
  connection.add(userBrokerLogTableId, logEntity->entity, id);
}
