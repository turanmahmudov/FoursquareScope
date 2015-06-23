#include <api/client.h>

#include <core/net/error.h>
#include <core/net/http/client.h>
#include <core/net/http/content_type.h>
#include <core/net/http/response.h>
#include <QVariantMap>

namespace http = core::net::http;
namespace net = core::net;

using namespace api;
using namespace std;

Client::Client(Config::Ptr config) :
    config_(config), cancelled_(false) {
}


void Client::get(const net::Uri::Path &path,
                 const net::Uri::QueryParameters &parameters, QJsonDocument &root) {
    // Create a new HTTP client
    auto client = http::make_client();

    // Start building the request configuration
    http::Request::Configuration configuration;

    // Build the URI from its components
    net::Uri uri = net::make_uri(config_->apiroot, path, parameters);
    configuration.uri = client->uri_to_string(uri);

    // Give out a user agent string
    configuration.header.add("User-Agent", config_->user_agent);

    // Build a HTTP request object from our configuration
    auto request = client->head(configuration);

    try {
        // Synchronously make the HTTP request
        // We bind the cancellable callback to #progress_report
        auto response = request->execute(
                    bind(&Client::progress_report, this, placeholders::_1));

        // Check that we got a sensible HTTP status code
        if (response.status != http::Status::ok) {
            throw domain_error(response.body);
        }
        // Parse the JSON from the response
        root = QJsonDocument::fromJson(response.body.c_str());

        // Open weather map API error code can either be a string or int
        QVariant cod = root.toVariant().toMap()["cod"];
        if ((cod.canConvert<QString>() && cod.toString() != "200")
                || (cod.canConvert<unsigned int>() && cod.toUInt() != 200)) {
            throw domain_error(root.toVariant().toMap()["message"].toString().toStdString());
        }
    } catch (net::Error &) {
    }
}

Client::TrackRes Client::tracks(const string& query, const string& ll) {
    QJsonDocument root;

    // Build a URI and get the contents.
    // The fist parameter forms the path part of the URI.
    // The second parameter forms the CGI parameters.

    get( { "v2", "venues", "explore" }, { { "near", ll }, { "query", query }, { "section", "topPicks" }, { "venuePhotos", "1" }, { "radius", "800" }, { "oauth_token", "BPFNADBLKIOPI3YQOILBRNF3DMZ1315PBMZS2XNETXUWU0SY" }, { "v", "20140926" }, { "limit", "10" } }, root);

    // My “list of tracks” object (as seen in the corresponding header file)
    TrackRes result;

    QVariantMap variant = root.toVariant().toMap();
    QVariantMap rData = variant["response"].toMap();
    QVariantList gData = rData["groups"].toList();
    QVariantMap gDataFirst = gData.first().toMap();
    QVariantList vList = gDataFirst["items"].toList();
    for (const QVariant &i : vList) {
        QVariantMap ite = i.toMap();
        QVariantMap item = ite["venue"].toMap();

        // Category
        QVariantMap category;
        QVariantList categories = item["categories"].toList();
        for (const QVariant &j : categories) {
            QVariantMap citem = j.toMap();
            category = citem;
        }
        QVariantMap category_icon = category["icon"].toMap();

        // Location
        QVariantMap location = item["location"].toMap();
        string addr = "";
        string address = location["address"].toString().toStdString();
        string crossStreet = location["crossStreet"].toString().toStdString();
        if (!address.empty()) {
            if (!crossStreet.empty()) {
                addr = address + " (" + crossStreet + ")";
            } else {
                addr = address;
            }
        } else {
            if (!crossStreet.empty()) {
                addr = crossStreet;
            }
        }

        // Photo
        string photo = category_icon["prefix"].toString().toStdString() + "bg_100" + category_icon["suffix"].toString().toStdString();
        QVariantMap fpData = item["featuredPhotos"].toMap();
        QVariantList fpDataItems = fpData["items"].toList();
        for (const QVariant &k : fpDataItems) {
            QVariantMap fpDataItemsFirst = fpDataItems.first().toMap();
            photo = fpDataItemsFirst["prefix"].toString().toStdString() + "304x304" + fpDataItemsFirst["suffix"].toString().toStdString();
        }

        // We add each result to our list
        result.tracks.emplace_back(
            Track {
                item["id"].toUInt(),
                item["name"].toString().toStdString(),
                category["name"].toString().toStdString(),
                category_icon["prefix"].toString().toStdString() + "bg_100" + category_icon["suffix"].toString().toStdString(),
                addr,
                photo,
                "uri"
            }
        );
    }

    // We add each result to our list
    result.tracks.emplace_back(
        Track {
            2,
            "bashliq"
            "uri",
            "bosh",
            "url",
            "",
            "uri"
        }
    );

    return result;
}

http::Request::Progress::Next Client::progress_report(
        const http::Request::Progress&) {

    return cancelled_ ?
                http::Request::Progress::Next::abort_operation :
                http::Request::Progress::Next::continue_operation;
}

void Client::cancel() {
    cancelled_ = true;
}

Config::Ptr Client::config() {
    return config_;
}

