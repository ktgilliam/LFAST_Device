#pragma once

#include <Arduino.h>
#include <StreamUtils.h>
#include "Client.h"
#include <ArduinoJson.h>

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstring>

#define MAX_ARGS 4
#define RX_BUFF_SIZE 1024

#define MAX_KV_PAIRS 20
#define JSON_PROGMEM_SIZE JSON_OBJECT_SIZE(MAX_KV_PAIRS)

#define MAX_CTRL_MESSAGES 0x40U // can be increased if needed

namespace LFAST
{
    ///////////////// TYPES /////////////////
    class CommsMessage
    {
    public:
        // CommsMessage(){}
        CommsMessage()
        {
            std::memset(this->jsonInputBuffer, 0, sizeof(this->jsonInputBuffer));
            processed = false;
        }
        virtual ~CommsMessage() {}
        virtual void placeholder() {}
        void printMessageInfo();
        std::string getMessageStr();
        StaticJsonDocument<JSON_PROGMEM_SIZE> &getJsonDoc()
        {
            return this->JsonDoc;
        }
        StaticJsonDocument<JSON_PROGMEM_SIZE> &deserialize();

        template <typename T>
        inline T getValue(const std::string &key);
        template <typename T>
        inline void addKeyValuePair(const std::string &key, T val);
        inline void addDestinationKey(const std::string &key);

        const char *getBuffPtr()
        {
            return jsonInputBuffer;
        };
        char jsonInputBuffer[JSON_PROGMEM_SIZE];
        void setProcessedFlag()
        {
            processed = true;
        }
        bool hasBeenProcessed()
        {
            return processed;
        }

    protected:
        StaticJsonDocument<JSON_PROGMEM_SIZE> JsonDoc;
        bool processed;
        std::string destKey;
    };

    template <class T>
    struct MessageHandler
    {
        void (*MsgHandlerFn)(T);

        MessageHandler()
        {
            this->MsgHandlerFn = nullptr;
        }

        MessageHandler(void (*ptr)(T))
        {
            this->MsgHandlerFn = ptr;
        }
        virtual ~MessageHandler(){};

        bool call(T val)
        {
            if (this->MsgHandlerFn)
            {
                MsgHandlerFn(val);
                return true;
            }
            return false;
        }
    };

    struct ClientConnection
    {
        ClientConnection(Client *_client) : client(_client), noReplyFlag(false) {}
        Client *client;
        bool noReplyFlag;
        std::vector<CommsMessage *> rxMessageQueue;
        std::vector<CommsMessage *> txMessageQueue;
    };
    class CommsService
    {

    protected:
        static void defaultMessageHandler(std::string);
        void errorMessageHandler(CommsMessage &msg);
        static std::vector<ClientConnection> connections;
        ClientConnection *activeConnection;
        bool commsServiceStatus;

    private:
        enum HandlerType
        {
            INT_HANDLER,
            UINT_HANDLER,
            DOUBLE_HANDLER,
            BOOL_HANDLER,
            STRING_HANDLER
        };
        std::unordered_map<std::string, HandlerType> handlerTypes;
        std::unordered_map<std::string, MessageHandler<int>> intHandlers;
        std::unordered_map<std::string, MessageHandler<unsigned int>> uIntHandlers;
        std::unordered_map<std::string, MessageHandler<double>> doubleHandlers;
        std::unordered_map<std::string, MessageHandler<bool>> boolHandlers;
        std::unordered_map<std::string, MessageHandler<std::string>> stringHandlers;
        template <class T>
        void callMessageHandler(const std::string &key, T val);

    public:
        CommsService();
        virtual ~CommsService() {}

        void setupClientMessageBuffers(Client *client);
        bool getNewMessages(ClientConnection &);
        enum
        {
            ACTIVE_CONNECTION = 1,
            ALL_CONNECTED = 2,
        };
        virtual void sendMessage(CommsMessage &, uint8_t);
        template <class T>
        inline void registerMessageHandler(const std::string &key, MessageHandler<T> fn);
        inline void callMessageHandler(JsonPair kvp);

        virtual bool Status()
        {
            return commsServiceStatus;
        };

        void checkForNewClientData();
        virtual bool checkForNewClients();
        virtual void stopDisconnectedClients();
        virtual void processClientData();
        virtual void processMessage(CommsMessage *);
        void setNoReplyFlag(bool f)
        {
            activeConnection->noReplyFlag = f;
        }
    };

    // NOTE: Teensy build environment doesn't handle build flags properly, so can't use typeid().
    // template <class T>
    // bool LFAST::CommsService::registerMessageHandler(const std::string & key, MessageHandler<T> fn)
    // {
    //     if (typeid(T) == typeid(int))
    //         this->intHandlers[key] = fn;
    //     else if (typeid(T) == typeid(unsigned int))
    //         this->uIntHandlers[key] = fn;
    //     else if (typeid(T) == typeid(double))
    //         this->doubleHandlers[key] = fn;
    //     else if (typeid(T) == typeid(bool))
    //         this->boolHandlers[key] = fn;
    //     else if (typeid(T) == typeid(std::string))
    //         this->stringHandlers[key] = fn;
    //     else
    //         return false;
    //     return true;
    // }

    template <class T>
    void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<T> fn)
    {
        // TODO: Add exception handling
    }

    template <>
    inline void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<int> fn)
    {
        this->intHandlers[key] = fn;
        this->handlerTypes[key] = INT_HANDLER;
    }
    template <>
    inline void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<unsigned int> fn)
    {
        this->uIntHandlers[key] = fn;
        this->handlerTypes[key] = UINT_HANDLER;
    }
    template <>
    inline void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<double> fn)
    {
        this->doubleHandlers[key] = fn;
        this->handlerTypes[key] = DOUBLE_HANDLER;
    }
    template <>
    inline void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<bool> fn)
    {
        this->boolHandlers[key] = fn;
        this->handlerTypes[key] = BOOL_HANDLER;
    }
    template <>
    inline void LFAST::CommsService::registerMessageHandler(const std::string &key, MessageHandler<std::string> fn)
    {
        this->stringHandlers[key] = fn;
        this->handlerTypes[key] = STRING_HANDLER;
    }

    template <>
    inline void LFAST::CommsService::callMessageHandler(const std::string &key, int val)
    {
        if (this->intHandlers.find(key) == this->intHandlers.end())
        {
            char errBuff[100];
            sprintf(errBuff, "Key not found: [%s][%d]", key, val);
            throw std::runtime_error(errBuff);
        }
        else
        {
            auto mh = this->intHandlers[key];
            mh.call(val);
        }
    }

    template <>
    inline void LFAST::CommsService::callMessageHandler(const std::string &key, unsigned int val)
    {
        if (this->intHandlers.find(key) == this->intHandlers.end())
        {
            char errBuff[100];
            sprintf(errBuff, "Key not found: [%s][%d]", key, val);
            throw std::runtime_error(errBuff);
        }
        else
        {
            auto mh = this->uIntHandlers[key];
            mh.call(val);
        }
    }

    template <>
    inline void LFAST::CommsService::callMessageHandler(const std::string &key, double val)
    {
        if (this->intHandlers.find(key) == this->intHandlers.end())
        {
            char errBuff[100];
            sprintf(errBuff, "Key not found: [%s][%d]", key, val);
            throw std::runtime_error(errBuff);
        }
        else
        {
            auto mh = this->doubleHandlers[key];
            mh.call(val);
        }
    }

    template <>
    inline void LFAST::CommsService::callMessageHandler(const std::string &key, bool val)
    {
        if (this->intHandlers.find(key) == this->intHandlers.end())
        {
            char errBuff[100];
            sprintf(errBuff, "Key not found: [%s][%d]", key, val);
            throw std::runtime_error(errBuff);
        }
        else
        {
            auto mh = this->boolHandlers[key];
            mh.call(val);
        }
    }

    template <>
    inline void LFAST::CommsService::callMessageHandler(const std::string &key, std::string val)
    {
        if (this->intHandlers.find(key) == this->intHandlers.end())
        {
            char errBuff[100];
            sprintf(errBuff, "Key not found: [%s][%d]", key, val);
            throw std::runtime_error(errBuff);
        }
        else
        {
            auto mh = this->stringHandlers[key];
            mh.call(val);
        }
    }

    template <>
    inline double CommsMessage::getValue(const std::string &key)
    {
        return (JsonDoc[key.c_str()].as<double>());
    }

    template <>
    inline int CommsMessage::getValue(const std::string &key)
    {
        return (JsonDoc[key.c_str()].as<int>());
    }

    template <>
    inline unsigned int CommsMessage::getValue(const std::string &key)
    {
        return (JsonDoc[key.c_str()].as<unsigned int>());
    }

    template <>
    inline bool CommsMessage::getValue(const std::string &key)
    {
        return (JsonDoc[key.c_str()].as<bool>());
    }

    template <>
    inline std::string CommsMessage::getValue(const std::string &key)
    {
        return (std::string(JsonDoc[key.c_str()].as<const char *>()));
    }

    inline void CommsMessage::addDestinationKey(const std::string &key)
    {

        // newDoc.createNestedObject(key);
        // newDoc[key] = JsonDoc.as<JsonObject>();
        destKey = key;

        if (!JsonDoc.isNull())
        {
            StaticJsonDocument<JSON_PROGMEM_SIZE> newDoc;

            for (JsonPairConst kvp : this->JsonDoc.as<JsonObjectConst>())
            {
                newDoc[key][kvp.key()] = kvp.value();
            }
            this->JsonDoc = newDoc;
        }
        else
        {
            JsonDoc.createNestedObject(destKey);
        }

        // = this->getJsonDoc();
        // JsonObject newObj = JsonDoc.
    }

    template <typename T>
    inline void CommsMessage::addKeyValuePair(const std::string &key, T val)
    {
        if (this->destKey.length() > 0)
            JsonDoc[this->destKey][key] = val;
        else
            JsonDoc[key] = val;
    };
}