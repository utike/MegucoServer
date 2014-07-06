
#pragma once

#include "Tools/Bot.h"

class ItemBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    struct Parameters
    {
      double sellProfitGain;
      double buyProfitGain;
      //double sellPriceGain;
      //double buyPriceGain;
    };

    Session(Broker& broker);

  private:
    Broker& broker;

    Parameters parameters;

    double minBuyInPrice;
    double maxSellInPrice;

    virtual ~Session() {}

    virtual void_t setParameters(double* parameters);

    virtual void_t handle(const DataProtocol::Trade& trade, const Values& values);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId);
    virtual void_t handleSellTimeout(uint32_t orderId);

    void_t checkBuy(const DataProtocol::Trade& trade, const Values& values);
    void_t checkSell(const DataProtocol::Trade& trade, const Values& values);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual uint_t getParameterCount() const {return sizeof(Session::Parameters) / sizeof(double);}
};
