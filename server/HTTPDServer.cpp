/*

  Copyright (C) 2011 Grame

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

  Grame Research Laboratory, 9 rue du Garet, 69001 Lyon - France
  research@grame.fr

*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <algorithm>

#include "utilities.h"
#include "HTTPDServer.h"
#include "guido2img.h"
#include "guidosession.h"

using namespace std;

#define COOKIE_NAME "guidoserver"

namespace guidohttpd
{

// for json parser
static int parsechannel(void *userdata, int type, const char *data, uint32_t length)
{
    TArgs* args = (TArgs*)userdata;
    switch (type) {
    case JSON_OBJECT_BEGIN:
        break;
    case JSON_ARRAY_BEGIN:
        break;
    case JSON_OBJECT_END:
        break;
    case JSON_ARRAY_END:
        break;
    case JSON_NULL:
        break;
    case JSON_KEY:
        args->push_back (TArg (data, ""));
        break;
    case JSON_STRING:
        args->back ().second = data;
        break;
    case JSON_INT:
        args->back ().second = data;
        break;
    case JSON_FLOAT:
        args->back ().second = data;
        break;
    case JSON_TRUE:
        args->back ().second = "true";
        break;
    case JSON_FALSE:
        args->back ().second = "false";
        break;
    }
    return 0;
}


//--------------------------------------------------------------------------
// static functions
// provided as callbacks to mhttpd
//--------------------------------------------------------------------------
static int _answer_to_connection (void *cls, struct MHD_Connection *connection, const char *url, const char *method, const char *version,
                                  const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    HTTPDServer* server = (HTTPDServer*)cls;
    return server->answer(connection, url, method, version, upload_data, upload_data_size, con_cls);
}

//--------------------------------------------------------------------------

void _request_completed (void *cls, struct MHD_Connection *connection,
                         void **con_cls,
                         enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct *con_info = (connection_info_struct *)(*con_cls);

    if (NULL == con_info) {
        return;
    }

    if (con_info->connectiontype == POST) {
        MHD_destroy_post_processor (con_info->postprocessor);
        //if (con_info->answerstring) free (con_info->answerstring);
    }

    free (con_info);
    *con_cls = NULL;
}

static int _get_params (void *cls, enum MHD_ValueKind , const char *key, const char *data)
{
    TArgs* args = (TArgs*)cls;
    TArg arg(key, (data ? data : ""));
    args->push_back (arg);
    return MHD_YES;
}


static int
_post_params (void *coninfo_cls, enum MHD_ValueKind , const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data,
              uint64_t off, size_t size)
{
    struct connection_info_struct *con_info = (connection_info_struct *)coninfo_cls;
    json_parser parser;
    int ret = json_parser_init(&parser, NULL, parsechannel, &(con_info->args));
    if (ret) {
        return MHD_NO;
    }
    if (strcmp (key, "data") == 0) {
        for (size_t i = 0; i < strlen(data); i++) {
            ret = json_parser_string(&parser, data + i, 1, NULL);
            if (ret) {
                return MHD_NO;
            }
        }
    }
    json_parser_free(&parser);
    return MHD_YES;
}

//--------------------------------------------------------------------------
// the http server
//--------------------------------------------------------------------------
HTTPDServer::HTTPDServer(int port, guido2img* g2svg)
    : fPort(port), fServer(0), fConverter(g2svg)
{
}

HTTPDServer::~HTTPDServer()
{
    for (map<string, guidouser *>::iterator it = fUsers.begin ();
            it != fUsers.end(); it++) {
        for (map<string, guidosession *>::iterator itt = it->second->fSessions.begin();
                itt != it->second->fSessions.end(); itt++) {
            delete itt->second;
        }
        delete it->second;
    }

    stop();
}

void
HTTPDServer::add_session_cookie (guidouser *session, MHD_Response *response)
{
    char cstr[256];
    snprintf (cstr,
              sizeof (cstr),
              "%s=%s",
              COOKIE_NAME,
              session->getCookie().c_str());
    if (MHD_NO ==
            MHD_add_response_header (response,
                                     MHD_HTTP_HEADER_SET_COOKIE,
                                     cstr)) {
        fprintf (stderr,
                 "Failed to set session cookie header!\n");
    }
}

//--------------------------------------------------------------------------
bool HTTPDServer::start(int port)
{
    fServer = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, _answer_to_connection, this, MHD_OPTION_NOTIFY_COMPLETED, &_request_completed, NULL, MHD_OPTION_END);
    return fServer != 0;
}

//--------------------------------------------------------------------------
void HTTPDServer::stop ()
{
    if (fServer) {
        MHD_stop_daemon (fServer);
    }
    fServer=0;
}

//--------------------------------------------------------------------------
int HTTPDServer::send (struct MHD_Connection *connection, const char *page, int length, const char* type, guidouser *session, int status)
{
    struct MHD_Response *response = MHD_create_response_from_buffer (length, (void *) page, MHD_RESPMEM_MUST_COPY);
    if (!response) {
        cerr << "MHD_create_response_from_buffer error: null response\n";
        return MHD_NO;
    }
    add_session_cookie (session, response);
    MHD_add_response_header (response, "Content-Type", type ? type : "text/plain");
    int ret = MHD_queue_response (connection, status, response);
    MHD_destroy_response (response);
    return ret;
}

//--------------------------------------------------------------------------
int HTTPDServer::send (struct MHD_Connection *connection, const char *page, const char* type, guidouser *session, int status)
{
    return send (connection, page, strlen (page), type, session, status);
}

//--------------------------------------------------------------------------
const char* HTTPDServer::getMIMEType (const string& page)
{
    size_t n = page.find_last_of ('.');
    if (n != string::npos) {
        string ext = page.substr (n+1);
        if (ext == "css")	{
            return "text/css";
        }
        if (ext == "html")	{
            return "text/html";
        }
        if (ext == "js")	{
            return "application/javascript";
        }
    }
    return "text/plain";		// default MIME type
}

//--------------------------------------------------------------------------
int HTTPDServer::sendGuido (struct MHD_Connection *connection, const char* url, const TArgs& args, int type)
{

    const char* fakecookie;
    string cookie;
    fakecookie = MHD_lookup_connection_value (connection,
                 MHD_COOKIE_KIND,
                 COOKIE_NAME);
    if (fakecookie == NULL) {
        stringstream mystream;
        mystream << random() << random() << random() << random();
        cookie = mystream.str().c_str();
    } else {
        cookie = fakecookie;
    }

    string suburl = string(url).substr(1,string::npos);
    guidosession *currentSession;
    // we first check to see if the user exists
    map<string, guidouser *>::iterator it;
    it = fUsers.find(cookie);
    if (it == fUsers.end ()) {
        fUsers[cookie] = new guidouser();
        fUsers[cookie]->setCookie(cookie);
    }
    // we then check to see if a given session for the user exists
    map<string, guidosession *>::iterator itt;
    itt = fUsers[cookie]->fSessions.find(suburl);
    if (itt == fUsers[cookie]->fSessions.end ()) {
        fUsers[cookie]->fSessions[suburl] = new guidosession(fConverter);
        fUsers[cookie]->fSessions[suburl]->initialize();
        fUsers[cookie]->fSessions[suburl]->setUrl(suburl);
    }
    currentSession = fUsers[cookie]->fSessions[suburl];

    unsigned int n = 0;
    guidosession::callback_function callback;
    guidosessionresponse response;

    if (type == DELETE) {
        /*
         * One possible response - delete an fSession from a given fUser.
         */
        if (!args.size () || args.size () > 1 || args[0].first != "score") {
            response.errstring_ = "There may be one and only one argument called \"score\" passed to DELETE.";
            response.data_ = response.errstring_.c_str();
            response.size_ = response.errstring_.size();
            response.http_status_ = 405;
        } else if (fUsers[cookie]->fSessions.find (args[0].second) == fUsers[cookie]->fSessions.end ()) {
            response.errstring_ = "Cannot delete the score "+args[0].second+" because it does not exist.";
            response.data_ = response.errstring_.c_str();
            response.size_ = response.errstring_.size();
            response.http_status_ = 404;
        } else {
            fUsers[cookie]->fSessions.erase (args[0].second);
            delete currentSession;
            response.errstring_ = "";
            string success = "Successfully removed the score "+args[0].second+".";
            response.data_ = success.c_str();
            response.size_ = success.size();
            response.http_status_ = 200;
        }
    } else if (type == GET) {
        /*
         * we make sure that this is really a get, meaning we screen POST
         */
        for (unsigned int i = 0; i < 11; i++) {
            string postcommands[11] = {"page", "width", "height", "marginleft","marginright","margintop", "marginbottom", "zoom", "resizepagetomusic", "gmn","format"};
            for (unsigned int j = 0; j < args.size(); j++)
                if (args[j].first == postcommands[i]) {
                    n = args.size(); // skip args
                    callback = &guidosession::handleErrantGet;
                    response = (currentSession->*callback)(args, n);
                    response.data_ = response.errstring_.c_str();
                    response.size_ = response.errstring_.size();
                    response.http_status_ = 405;
                    break;
                }
            if (callback) {
                break;
            }
        }
    } else if (type == POST) {
        /*
         * we make sure that this is really a post, meaning we screen GET
         */
        for (unsigned int i = 0; i < args.size(); i++)
            if (args[i].first == "get") {
                n = args.size(); // skip args
                callback = &guidosession::handleErrantPost;
                response = (currentSession->*callback)(args, n);
                response.data_ = response.errstring_.c_str();
                response.size_ = response.errstring_.size();
                response.http_status_ = 405;
                break;
            }
    }

    while (n < args.size()) {
        if (args[n].first == "get") {
            callback = &guidosession::handleGet;
        } else if (args[n].first == "page") {
            callback = &guidosession::handlePage;
        } else if (args[n].first == "width") {
            callback = &guidosession::handleWidth;
        } else if (args[n].first == "height") {
            callback = &guidosession::handleHeight;
        } else if (args[n].first == "marginleft") {
            callback = &guidosession::handleMarginLeft;
        } else if (args[n].first == "marginright") {
            callback = &guidosession::handleMarginRight;
        } else if (args[n].first == "margintop") {
            callback = &guidosession::handleMarginTop;
        } else if (args[n].first == "marginbottom") {
            callback = &guidosession::handleMarginBottom;
        } else if (args[n].first == "zoom") {
            callback = &guidosession::handleZoom;
        } else if (args[n].first == "resizepagetomusic") {
            callback = &guidosession::handleResizePageToMusic;
        } else if (args[n].first == "gmn") {
            callback = &guidosession::handleGMN;
        } else if (args[n].first == "format") {
            callback = &guidosession::handleFormat;
        } else if (args[n].first == "") {
            callback = &guidosession::handleBlankRequest;
        } else {
            callback = &guidosession::handleFaultyInput;
        }

        response = (currentSession->*callback)(args, n);

        if (response.status_ == GUIDO_SESSION_PARSING_SUCCESS) {
            n += response.argumentsToAdvance_;
        } else {
            n += 1;
            response.data_ = response.errstring_.c_str();
            response.size_ = response.errstring_.size();
        }
    }

    if (suburl.size() == 0) {
        currentSession->initialize();
    }

    // Only the final result gets sent.
    const char* formatToSend = response.format_.c_str();
    return send (connection, response.data_, response.size_, formatToSend, fUsers[cookie], response.http_status_);
}

//--------------------------------------------------------------------------
int HTTPDServer::answer (struct MHD_Connection *connection, const char *url, const char *method, const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    const union MHD_ConnectionInfo * infos = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    log << infos->client_addr << " - " << method << " - " << url << logend;
    // <<---- BEGIN POST TESTING
    if (NULL == *con_cls) {
        struct connection_info_struct *con_info = new connection_info_struct ();
        if (0 == strcmp (method, "POST")) {
            con_info->postprocessor =
                MHD_create_post_processor (connection, 1024, // arbitrary, recommeneded by libmicrohttpd
                                           _post_params, (void *) con_info);

            if (NULL == con_info->postprocessor) {
                delete con_info;
                return MHD_NO;
            }
            /*
              The connectiontype field of con_info currently does nothing:
             it's a placeholder for using POST/GET distinctions as the server
             becomes more sophisticated.

             */
            con_info->connectiontype = POST;
        } else {
            con_info->connectiontype = GET;
        }

        *con_cls = (void *) con_info;

        return MHD_YES;
    }

    if (0 == strcmp (method, "POST")) {
        struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

        if (*upload_data_size != 0) {
            MHD_post_process (con_info->postprocessor, upload_data,
                              *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        } else {
            struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;
#ifdef __MACH__
            reverse (con_info->args.begin (), con_info->args.end ());
#endif
            return sendGuido (connection, url, con_info->args, POST);
        }
    }
    if (0 == strcmp (method, "GET")) {
        TArgs args;
        MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, _get_params, &args);
#ifdef __MACH__
        reverse (args.begin(), args.end());
#endif
        return sendGuido (connection, url, args, GET);
    }

    if (0 == strcmp (method, "DELETE")) {
        TArgs args;
        MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, _get_params, &args);
#ifdef __MACH__
        reverse (args.begin(), args.end());
#endif
        return sendGuido (connection, url, args, DELETE);
    }
    return MHD_YES;
}

} // end namespoace