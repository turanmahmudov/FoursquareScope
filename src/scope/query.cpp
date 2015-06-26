#include <boost/algorithm/string/trim.hpp>

#include <scope/localization.h>
#include <scope/query.h>

#include <unity/scopes/Annotation.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/CategoryRenderer.h>
#include <unity/scopes/QueryBase.h>
#include <unity/scopes/SearchReply.h>
#include <unity/scopes/SearchMetadata.h>

#include <unity/scopes/Location.h>

#include <iomanip>
#include <sstream>

namespace sc = unity::scopes;
namespace alg = boost::algorithm;

using namespace std;
using namespace api;
using namespace scope;


/**
 * Define the layout for the forecast results
 *
 * The icon size is small, and ask for the card layout
 * itself to be horizontal. I.e. the text will be placed
 * next to the image.
 */

const static string VENUES_TEMPLATE =
    R"(
        {
            "schema-version": 1,
            "template": {
                "category-layout": "grid",
                "card-layout": "horizontal",
                "card-size": "small"
            },
            "components": {
                "title": "title",
                "art" : {
                    "field": "art"
                },
                "subtitle": "category"
            }
        }
    )";

const static std::string LOGIN_TEMPLATE =
        R"(
{
        "schema-version": 1,
        "template": {
        "category-layout": "grid",
        "card-layout": "vertical",
        "card-size": "large",
        "card-background": "color:///white"
        },
        "components": {
        "title": "title",
        "mascot": "art"
        }
        }
        )";


Query::Query(const sc::CannedQuery &query, const sc::SearchMetadata &metadata,
             Config::Ptr config) :
    sc::SearchQueryBase(query, metadata), client_(config) {
}

void Query::cancelled() {
    client_.cancel();
}


void Query::run(sc::SearchReplyProxy const& reply) {
    try {
        // Start by getting information about the query
        const sc::CannedQuery &query(sc::SearchQueryBase::query());

        // A string to store the location name for the openweathermap query
        std::string ll;
        std::string lol;

        // Access search metadata
        auto metadata = search_metadata();

        // Check for location data
        if(metadata.has_location()) {
            auto location = metadata.location();

            // Check for city and country
            if(location.latitude() && location.longitude()) {

                // Create the "city country" string
                //auto lat = location.latitude();
                //auto lng = location.longitude();

                //ll = location.country_name();
                //ll = std::to_string(lat) + "," + std::to_string(lng);

                // Must be edit
                auto lat = location.latitude();
                auto lng = location.longitude();

                // Buffers for future strings
                char buflat[7];
                char buflng[7];

                // Keep two decimal points
                sprintf(buflat, "%.2f", lat);
                sprintf(buflng, "%.2f", lng);

                // Convert to std::string
                std::string latstr(buflat);
                std::string lngstr(buflng);

                string rlatstr = latstr.replace(2, 1, ".");
                string rlngstr = lngstr.replace(2, 1, ".");
                ll = rlatstr + "," + rlngstr;
            }
        }

        // Fallback to a hardcoded location
        if(ll.empty()) {
            ll = "40.37,49.84";
        }

        // Trim the query string of whitespace
        string query_string = alg::trim_copy(query.query_string());

        Client::TrackRes trackslist;
        if (query_string.empty()) {
            // If the string is empty, provide a specific one
            trackslist = client_.tracks("", ll);
        } else {
            // otherwise, use the query string
            trackslist = client_.tracks(query_string, ll);
        }


        // Register a category for tracks
        auto tracks_cat = reply->register_category("tracks", "", "",
            sc::CategoryRenderer(VENUES_TEMPLATE));
        // register_category(arbitrary category id, header title, header icon, template)
        // In this case, since this is the only category used by our scope,
        // it doesn’t need to display a header title, we leave it as a blank string.


        for (const auto &track : trackslist.tracks) {

            // Use the tracks category
            sc::CategorisedResult res(tracks_cat);

            // We must have a URI
            res.set_uri(track.uri);

            // Our result also needs a track title
            res.set_title(track.title);

            // Set the rest of the attributes, art, artist, etc.
            res.set_art(track.category_icon);
            res["category"] = track.category_name;
            res["address"] = track.address;
            res["venue_photo"] = track.venue_photo;


            // Push the result
            if (!reply->push(res)) {
                // If we fail to push, it means the query has been cancelled.
                return;
            }
        }

    } catch (domain_error &e) {
        // Handle exceptions being thrown by the client API
        cerr << e.what() << endl;
        reply->error(current_exception());

    }
}

