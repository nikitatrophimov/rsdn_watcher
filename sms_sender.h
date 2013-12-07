#ifndef SMS_SENDER_H
#define SMS_SENDER_H

#ifndef Q_MOC_RUN
#include <boost/config.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#endif // Q_MOC_RUN

#include <curl/curl.h>

#include <exception>
#include <string>

#define TWILIO_API_URL		"https://api.twilio.com"
#define TWILIO_API_VERSION	"2010-04-01"

class SmsSenderException : public std::exception
{
public:
    SmsSenderException(const std::string& msg) : _msg(msg) {}

    /* virtual */ const char* what() const override BOOST_NOEXCEPT
    {
        return _msg.c_str();
    }

private:
    const std::string _msg;
};

inline
int writer(char* data, std::size_t size, std::size_t nmemb, std::string* buffer)
{
    int result = 0;
    if (buffer != NULL)
    {
        buffer->append(data, size * nmemb);
        result = size * nmemb;
    }
    return result;
}

class SmsSender
{
public:
    SmsSender();

    void init();
    void cleanup();

    void set_twilio_account_sid(const std::string& twilio_account_sid);
    void set_twilio_account_token(const std::string& twilio_account_token);
    void set_twilio_from_phone(const std::string& twilio_from_phone);

    bool send_sms(
        const std::string& to_phone
        , const std::string& message
    );

private:
    void init_curl();
    void cleanup_curl();
    void set_options_for_easy_curl(CURL* curl, curl_httppost* formpost, curl_slist* headerlist, std::string& response);
    std::string build_uri();
    void check_twilio_response(const std::string& response);

    std::string         _url;

    std::string         _twilio_account_sid;
    std::string         _twilio_account_token;
    std::string         _twilio_from_phone;

    const std::size_t	_amount_of_sending_attempts;
    const std::size_t	_sending_interval_on_failure_in_seconds;
};

#endif // SMS_SENDER_H
