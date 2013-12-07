#include "sms_sender.h"

#include "auxiliary.h"

#ifndef Q_MOC_RUN
#include <boost/chrono.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/thread.hpp>
#endif // Q_MOC_RUN

#include <curl/curl.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

SmsSender::SmsSender() :
    _amount_of_sending_attempts(5)
  , _sending_interval_on_failure_in_seconds(5) {}

void SmsSender::init()
{
    init_curl();
}

void SmsSender::cleanup()
{
    cleanup_curl();
}

void SmsSender::init_curl()
{
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_global_init. Error code: " << res
        );
    }
}

void SmsSender::cleanup_curl()
{
    curl_global_cleanup();
}

void SmsSender::set_twilio_account_sid(const std::string& twilio_account_sid)
{
    _twilio_account_sid = twilio_account_sid;
}

void SmsSender::set_twilio_account_token(const std::string& twilio_account_token)
{
    _twilio_account_token = twilio_account_token;
}

void SmsSender::set_twilio_from_phone(const std::string& twilio_from_phone)
{
    _twilio_from_phone = twilio_from_phone;
}

bool SmsSender::send_sms(
    const std::string& to_phone
    , const std::string& message
)
{
    const boost::uuids::uuid message_uuid = boost::uuids::random_generator()();

    for (std::size_t i = 0; i < _amount_of_sending_attempts; ++i)
    {
        try
        {
            BOOST_LOG_TRIVIAL(info) << "Trying to send sms to " << to_phone << " | Message: " << message << " | uuid: " << message_uuid;

            _url = build_uri();

            std::vector<std::pair<std::string, std::string>> parameters;
            parameters.push_back(std::make_pair("To", to_phone));
            parameters.push_back(std::make_pair("From", _twilio_from_phone));
            parameters.push_back(std::make_pair("Body", message));

            CURLcode res;

            curl_httppost* formpost = NULL;
            curl_httppost* lastptr = NULL;
            const char buf[] = "Expect:";

            for (const auto& parameter : parameters)
            {
                CURLFORMcode formadd_res = curl_formadd(&formpost,
                    &lastptr,
                    CURLFORM_COPYNAME, parameter.first.c_str(),
                    CURLFORM_COPYCONTENTS, parameter.second.c_str(),
                    CURLFORM_END
                );
                if (formadd_res)
                {
                    throw SmsSenderException(
                        MakeString() << "An error occurred while using function curl_formadd. Error code: " << formadd_res
                    );
                }
            }

            // Инициализируем и настраиваем CURL easy interface
            CURL* curl = curl_easy_init();
            if (!curl)
            {
                throw SmsSenderException("An error occurred while using function curl_easy_init");
            }
            curl_slist* headerlist = NULL;
            headerlist = curl_slist_append(headerlist, buf);
            std::string response;
            set_options_for_easy_curl(curl, formpost, headerlist, response);

            res = curl_easy_perform(curl);
            if (res)
            {
                throw SmsSenderException(
                    MakeString() << "An error occurred while using function curl_easy_perform. Error code: " << res
                );
            }

            curl_easy_cleanup(curl);
            curl_formfree(formpost);
            curl_slist_free_all (headerlist);

            BOOST_LOG_TRIVIAL(info) << "Response for " << message_uuid << ": " << response;

            check_twilio_response(response);

            BOOST_LOG_TRIVIAL(info) << "SMS with uuid " << message_uuid << " was successfully sent";
            return true;
        }
        catch (const SmsSenderException& e)
        {
            std::ostringstream error_osstr;
            error_osstr << "An error occurred while sending sms for message with uuid == " << message_uuid << ": " << e.what();
            if (i < _amount_of_sending_attempts - 1)
            {
                error_osstr << ", trying again after " << _sending_interval_on_failure_in_seconds << " seconds...";
            }
            BOOST_LOG_TRIVIAL(error) << error_osstr.str();
            boost::this_thread::sleep_for(boost::chrono::seconds(_sending_interval_on_failure_in_seconds));
        }
    }

    return false;
}

std::string SmsSender::build_uri()
{
    return std::string(TWILIO_API_URL) + "/" + TWILIO_API_VERSION + "/Accounts/" + _twilio_account_sid + "/SMS/Messages";
}

void SmsSender::check_twilio_response(const std::string& response)
{
    boost::property_tree::ptree response_pt;
    std::stringstream response_ostr;
    response_ostr << response;
    try
    {
        boost::property_tree::read_xml(response_ostr, response_pt);
    }
    catch (const boost::property_tree::xml_parser_error& e)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while reading xml: " << e.what()
        );
    }

    boost::optional<boost::property_tree::ptree&> child = response_pt.get_child_optional("TwilioResponse.RestException");
    if (!child)
    {
        // Всё нормально
        return;
    }

    std::string twilio_exception_status;
    std::string twilio_exception_message;
    try
    {
        twilio_exception_status = child->get<std::string>("Status");
        twilio_exception_message = child->get<std::string>("Message");
    }
    catch (const boost::property_tree::ptree_bad_data& e)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while getting status / message of twilio exception: " << e.what()
        );
    }
    catch (const boost::property_tree::ptree_bad_path& e)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while getting status / message of twilio exception: " << e.what()
        );
    }

    boost::optional<std::string> twilio_exception_code = child->get_optional<std::string>("Code");
    boost::optional<std::string> twilio_exception_more_info = child->get_optional<std::string>("MoreInfo");

    std::string exception_message;
    exception_message += "Status: " + twilio_exception_status
                      + " | Message: " + twilio_exception_message;
    if (twilio_exception_code)
    {
        exception_message += " | Code: " + *twilio_exception_code;
    }
    if (twilio_exception_more_info)
    {
        exception_message += " | More Info: " + *twilio_exception_more_info;
    }

    throw SmsSenderException(exception_message);
}

void SmsSender::set_options_for_easy_curl(CURL* curl, curl_httppost* formpost, curl_slist* headerlist, std::string& response)
{
    CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, _url.c_str());
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_URL. Error code: " << res
        );
    }

    res = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_VERBOSE. Error code: " << res
        );
    }

    res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_SSL_VERIFYPEER. Error code: " << res
        );
    }
    res = curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_HTTPAUTH. Error code: " << res
        );
    }

    const std::string s_auth = _twilio_account_sid + ":" + _twilio_account_token;
    res = curl_easy_setopt(curl, CURLOPT_USERPWD, s_auth.c_str());
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_USERPWD. Error code: " << res
        );
    }
    res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_HTTPHEADER. Error code: " << res
        );
    }
    res = curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_HTTPPOST. Error code: " << res
        );
    }
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_WRITEFUNCTION. Error code: " << res
        );
    }
    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if (res != CURLE_OK)
    {
        throw SmsSenderException(
            MakeString() << "An error occurred while using function curl_easy_setopt for setting option CURLOPT_WRITEDATA. Error code: " << res
        );
    }
}
