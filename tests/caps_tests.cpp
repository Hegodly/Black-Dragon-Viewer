#include "kami/protocol/caps.hpp"
#include <objbase.h>
#include <winhttp.h>
#include <string>
using namespace kami::protocol::caps;
namespace {
int exercise(){
    const std::wstring trusted=
        L"https://simhost-123.agni.secondlife.io:12043/cap/abc";
    if(!trusted_caps_url(trusted))return 1;
    if(!trusted_caps_url(L"https://sim1234.agni.lindenlab.com:12043/cap/x"))
        return 2;
    for(const auto* url:{
        L"http://simhost-123.agni.secondlife.io:12043/cap/abc",
        L"https://localhost/cap/abc",
        L"https://127.0.0.1/cap/abc",
        L"https://lindenlab.com.evil.example/cap/abc",
        L"https://simhost-123.agni.secondlife.io:12043/",
        L"https://simhost-123.agni.secondlife.io:12043/cap/x#frag",
        L"https://user@simhost-123.agni.secondlife.io/cap/x"
    })if(trusted_caps_url(url))return 3;
    // Separate rules: EventQueue stays on Linden infrastructure;
    // authenticated ViewerAsset can use a public HTTPS CDN endpoint.
    if(trusted_caps_url(L"https://d111111abcdef8.cloudfront.net/")||
       !trusted_mesh_caps_url(
            L"https://d111111abcdef8.cloudfront.net/")||
       !trusted_mesh_caps_url(
            L"https://asset-cdn.glb.agni.lindenlab.com")||
       !trusted_mesh_caps_url(
            L"https://cdn.assets.example-real-vendor.com/mesh/endpoint"))
        return 32;
    for(const auto* unsafe:{
         L"http://d111111abcdef8.cloudfront.net/",
         L"https://127.0.0.1/asset",
         L"https://localhost/asset",
         L"https://metadata.google.internal/asset",
         L"https://cdn.example.org/asset",
         L"https://cdn.example.com/asset",
         L"https://asset.test/mesh",
         L"https://user@cdn.cloudfront.net/asset",
         L"https://a..b.net/asset",
         L"https://-bad.cloudfront.net/asset",
         L"https://cdn.cloudfront.net/#fragment"})
        if(trusted_mesh_caps_url(unsafe))return 33;
    const std::string cdn_seed=
        "<llsd><map><key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/events</string>"
        "<key>ViewerAsset</key>"
        "<uri>https://d111111abcdef8.cloudfront.net</uri>"
        "</map></llsd>";
    std::wstring cdn_event,cdn_asset;
    if(!parse_seed_llsd(cdn_seed,cdn_event,&cdn_asset)||
       cdn_event!=L"https://simhost-123.agni.secondlife.io/cap/events"||
       cdn_asset!=L"https://d111111abcdef8.cloudfront.net")
        return 34;
    // SL capability spec permits http: or https:. The historical
    // GetMesh/ViewerAsset CDN was http://asset-cdn.aditi.lindenlab.com/.
    // Allow only an authenticated Linden-owned public CDN root, never
    // arbitrary HTTP hosts, port changes, paths, or query bearer tokens.
    if(!trusted_mesh_caps_url(
            L"http://asset-cdn.aditi.lindenlab.com/")||
       !trusted_mesh_caps_url(
            L"http://asset-cdn.glb.agni.lindenlab.com")||
       !trusted_mesh_caps_url(
            L"http://asset-cdn.aditi.lindenlab.com:80/")||
       trusted_caps_url(L"http://asset-cdn.aditi.lindenlab.com/"))
        return 38;
    for(const auto* forbidden:{
        L"http://cdn.public-assets.org/",
        L"http://asset-cdn.aditi.lindenlab.com:8080/",
        L"http://asset-cdn.aditi.lindenlab.com/secret",
        L"http://asset-cdn.aditi.lindenlab.com/?token=topsecret",
        L"http://asset-cdn.aditi.lindenlab.com/?mesh_id=123",
        L"http://asset-cdn.evil-lindenlab.com/",
        L"http://asset-cdn.agni.lindenlab.com.evil.org/",
        L"http://localhost/",
        L"http://127.0.0.1/",
        L"http://asset-cdn.aditi.lindenlab.com/#fragment",
        L"http://user@asset-cdn.aditi.lindenlab.com/",
        L"http://asset-cdn.aditi.lindenlab.com\n"})
        if(trusted_mesh_caps_url(forbidden))return 39;
    const std::string legacy_seed=
        "<llsd><map><key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/events</string>"
        "<key>ViewerAsset</key>"
        "<uri>http://asset-cdn.aditi.lindenlab.com/</uri></map></llsd>";
    std::wstring legacy_event,legacy_mesh;
    SeedParseReport legacy_report{};
    if(!parse_seed_llsd(legacy_seed,legacy_event,&legacy_mesh,
                        &legacy_report)||
       legacy_event!=L"https://simhost-123.agni.secondlife.io/cap/events"||
       legacy_mesh!=L"http://asset-cdn.aditi.lindenlab.com/"||
       legacy_report.mesh_rejected)return 41;
    const std::string seed=
        "<?xml version=\"1.0\"?><llsd><map>"
        "<key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io:12043/cap/abc</string>"
        "<key>ViewerAsset</key><string>https://simhost-123.agni.secondlife.io/cap/z</string>"
        "</map></llsd>";
    std::wstring cap;
    std::wstring mesh_cap;
    if(!parse_seed_llsd(seed,cap,&mesh_cap)||cap!=trusted||
       mesh_cap!=L"https://simhost-123.agni.secondlife.io/cap/z")return 4;
    SeedParseReport parse_report{};
    if(parse_seed_llsd("<llsd><map><key>EventQueueGet</key>"
        "<string>http://localhost/cap/no</string></map></llsd>",
        cap,&mesh_cap,&parse_report)||!parse_report.xml_map||
       !parse_report.event_rejected)return 5;
    if(parse_seed_llsd("<!DOCTYPE x [ <!ENTITY x SYSTEM "
         "\"file:///etc/passwd\"> ]><llsd><map></map></llsd>",cap))
        return 6;
    unsigned count=255;
    const std::string empty=
        "<llsd><map><key>id</key><integer>12345</integer>"
        "<key>events</key><array/></map></llsd>";
    if(!parse_event_queue_llsd(empty,count)||count!=0)return 7;
    const std::string events=
        "<llsd><map><key>id</key><integer>12345</integer>"
        "<key>events</key><array><map><key>message</key>"
        "<string>EnableSimulator</string></map></array></map></llsd>";
    if(!parse_event_queue_llsd(events,count)||count!=1)return 8;
    if(parse_event_queue_llsd(
        "<llsd><map><key>events</key><array/></map></llsd>",count))
        return 9;
    if(parse_event_queue_llsd(
        "<llsd><map><key>id</key><integer>1</integer>"
        "<key>events</key><string>bad</string></map></llsd>",count))
        return 10;
    EventQueueFrame frame{};
    if(!parse_event_queue_frame(empty,frame)||
       frame.id.token!="12345"||!frame.id.integer||
       frame.event_count!=0)return 11;
    if(build_event_queue_post(frame.id).find(
         "<key>ack</key><integer>12345</integer>")==std::string::npos)
        return 12;
    if(build_event_queue_post({}).find(
         "<key>ack</key><undef />")==std::string::npos)
        return 13;
    if(!parse_event_queue_frame(events,frame)||
       frame.event_count!=1||frame.event_types.size()!=1||
       frame.event_types[0]!="EnableSimulator")return 14;
    const std::string typed=
        "<llsd><map><key>id</key><string>abc-1</string>"
        "<key>events</key><array/></map></llsd>";
    if(!parse_event_queue_frame(typed,frame)||
       frame.id.token!="abc-1"||frame.id.integer||
       build_event_queue_post(frame.id).find(
         "<key>ack</key><string>abc-1</string>")==std::string::npos)
        return 15;
    if(!build_event_queue_post({"x<evil",false}).empty()||
       !build_event_queue_post({"oops",true}).empty())return 16;
    if(parse_event_queue_frame(
        "<llsd><map><key>id</key><integer>4</integer>"
        "<key>events</key><array><string>untrusted</string></array>"
        "</map></llsd>",frame))return 17;
    const std::string unsafe=
       "<llsd><map><key>id</key><integer>5</integer>"
       "<key>events</key><array><map><key>message</key>"
       "<string>https://signed-cap.example/x</string></map></array>"
       "</map></llsd>";
    if(!parse_event_queue_frame(unsafe,frame)||
       frame.event_types.size()!=1||
       frame.event_types[0]!="UnknownEvent")return 18;
    // Regression: R8/R9 froze after the first legitimate event batch.
    // R9 fix safely accepts bounded metadata and advances the ACK cursor.
    const auto received=observe_event_queue_response(events);
    if(!received.structurally_valid||!received.safe_to_ack||
       received.event_count!=1||received.unsupported_events!=1||
       received.event_types.size()!=1||
       received.event_types[0]!="EnableSimulator"||
       received.next_ack.token!="12345")return 19;
    if(build_event_queue_post(received.next_ack).find(
        "<key>ack</key><integer>12345</integer>")==std::string::npos)
        return 20;
    const std::string two=
        "<llsd><map><key>id</key><integer>3001</integer>"
        "<key>events</key><array>"
        "<map><key>message</key><string>EnableSimulator</string>"
        "<key>body</key><map/></map>"
        "<map><key>message</key><string>EstablishAgentCommunication</string>"
        "<key>body</key><map/></map></array></map></llsd>";
    const auto batch=observe_event_queue_response(two);
    if(!batch.structurally_valid||!batch.safe_to_ack||
       batch.event_count!=2||batch.unsupported_events!=2||
       batch.next_ack.token!="3001"||batch.event_types.size()!=2||
       batch.event_types[1]!="EstablishAgentCommunication")return 21;
    if(build_event_queue_post(batch.next_ack).find(
        "<integer>3001</integer>")==std::string::npos)return 22;
    const auto invalid=observe_event_queue_response(
        "<llsd><map><key>id</key><undef />"
        "<key>events</key><array><map><key>message</key>"
        "<string>EnableSimulator</string></map></array></map></llsd>");
    if(invalid.structurally_valid||invalid.safe_to_ack)return 23;
    const auto malformed=observe_event_queue_response(
        "<!DOCTYPE x [<!ENTITY x SYSTEM \"file:///C:/secret\">]>"
        "<llsd><map><key>id</key><integer>1</integer>"
        "<key>events</key><array/></map></llsd>");
    if(malformed.structurally_valid||malformed.safe_to_ack)return 24;
    const auto no_event=observe_event_queue_response(empty);
    if(!no_event.structurally_valid||!no_event.safe_to_ack||
       no_event.event_count!=0||no_event.unsupported_events!=0)
        return 25;
    // Regression: WinHTTP 12152 must not permanently block the live
    // EventQueueGet loop. Only a long-held failure can be treated as
    // a possible idle upstream reply; no ACK is advanced on this path.
    if(!retryable_event_queue_http_error(ERROR_WINHTTP_INVALID_SERVER_RESPONSE)||
       !retryable_event_queue_http_error(ERROR_WINHTTP_TIMEOUT)||
       retryable_event_queue_http_error(ERROR_WINHTTP_SECURE_FAILURE))
        return 26;
    if(!possible_idle_event_queue_http_error(
            ERROR_WINHTTP_INVALID_SERVER_RESPONSE,true)||
       possible_idle_event_queue_http_error(
            ERROR_WINHTTP_INVALID_SERVER_RESPONSE,false)||
       possible_idle_event_queue_http_error(
            ERROR_WINHTTP_SECURE_FAILURE,true))
        return 27;
    // Independent grant negotiation: an invalid or absent optional mesh
    // URL must not erase a valid EventQueueGet bearer capability.
    const auto optional_bad=
        "<llsd><map><key>EventQueueGet</key>"
        "<uri>https://simhost-123.agni.secondlife.io/cap/events</uri>"
        "<key>ViewerAsset</key>"
        "<string>https://example.org/mesh</string></map></llsd>";
    if(!parse_seed_llsd(optional_bad,cap,&mesh_cap,&parse_report)||
       cap!=L"https://simhost-123.agni.secondlife.io/cap/events"||
       !mesh_cap.empty()||!parse_report.mesh_rejected)return 28;
    // A simulator may supply mesh but not EventQueue. Keep the usable
    // mesh capability rather than failing the entire seed response.
    const auto mesh_only=
        "<llsd><map><key>ViewerAsset</key>"
        "<uri>https://simhost-123.agni.secondlife.io/cap/mesh</uri>"
        "<key>EventQueueGet</key><undef/></map></llsd>";
    if(!parse_seed_llsd(mesh_only,cap,&mesh_cap,&parse_report)||
       !cap.empty()||
       mesh_cap!=L"https://simhost-123.agni.secondlife.io/cap/mesh"||
       !parse_report.event_rejected)return 29;
    // Prefer ViewerAsset when present; otherwise prefer GetMesh2 to GetMesh.
    const auto alternate=
        "<llsd><map><key>GetMesh</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/legacy</string>"
        "<key>GetMesh2</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/new</string>"
        "</map></llsd>";
    if(!parse_seed_llsd(alternate,cap,&mesh_cap)||
       !cap.empty()||
       mesh_cap!=L"https://simhost-123.agni.secondlife.io/cap/new")
       return 30;
    // An empty or syntactically invalid seed never grants a service.
    if(parse_seed_llsd("<llsd><map/></llsd>",cap,&mesh_cap,&parse_report)||
       !parse_report.xml_map)return 31;
    const std::string inventory_reply=
        "<llsd><map><key>folders</key><array><map>"
        "<key>folder_id</key><uuid>aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa</uuid>"
        "<key>version</key><integer>7</integer>"
        "<key>categories</key><array><map>"
        "<key>category_id</key><uuid>bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb</uuid>"
        "<key>parent_id</key><uuid>aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa</uuid>"
        "<key>name</key><string>Clothing</string></map></array>"
        "<key>items</key><array><map>"
        "<key>item_id</key><uuid>cccccccc-cccc-4ccc-8ccc-cccccccccccc</uuid>"
        "<key>parent_id</key><uuid>aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa</uuid>"
        "<key>name</key><string>Jacket</string></map></array>"
        "</map></array></map></llsd>";
    InventoryFolderReply decoded_inventory{};
    if(!parse_inventory_folder_llsd(inventory_reply,
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",decoded_inventory)||
       !decoded_inventory.valid||decoded_inventory.version!=7||
       decoded_inventory.entries.size()!=2||
       !decoded_inventory.entries[0].folder||
       decoded_inventory.entries[0].name!=L"Clothing"||
       decoded_inventory.entries[1].folder||
       decoded_inventory.entries[1].name!=L"Jacket")return 47;
    if(parse_inventory_folder_llsd(inventory_reply,
        "dddddddd-dddd-4ddd-8ddd-dddddddddddd",decoded_inventory))return 48;
    const auto inventory_request=build_inventory_folder_post(
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
        "12345678-90ab-4cde-8fab-000000000001",true,true);
    if(inventory_request.find("<key>folder_id</key><string>aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa</string>")==std::string::npos||
       inventory_request.find("<key>owner_id</key><string>12345678-90ab-4cde-8fab-000000000001</string>")==std::string::npos||
       inventory_request.find("<key>fetch_folders</key><boolean>true</boolean>")==std::string::npos||
       inventory_request.find("<key>fetch_items</key><boolean>true</boolean>")==std::string::npos||
       !build_inventory_folder_post("not-a-uuid","12345678-90ab-4cde-8fab-000000000001").empty())
        return 46;
    // Inventory CAPS are negotiated separately from mesh/EventQueue and
    // remain bearer URLs; only Linden-owned simulator HTTPS endpoints pass.
    const auto inventory_seed=
        "<llsd><map><key>FetchInventoryDescendents2</key>"
        "<uri>https://simhost-123.agni.secondlife.io/cap/inventory-folders</uri>"
        "<key>FetchInventory2</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/inventory-items</string>"
        "<key>EventQueueGet</key><undef/></map></llsd>";
    std::wstring inv_event,inv_mesh,inv_desc,inv_items;
    if(!parse_seed_llsd(inventory_seed,inv_event,&inv_mesh,nullptr,nullptr,
                        &inv_desc,&inv_items)||!inv_event.empty()||
       !inv_mesh.empty()||
       inv_desc!=L"https://simhost-123.agni.secondlife.io/cap/inventory-folders"||
       inv_items!=L"https://simhost-123.agni.secondlife.io/cap/inventory-items")
        return 44;
    const auto unsafe_inventory_seed=
        "<llsd><map><key>FetchInventoryDescendents2</key>"
        "<uri>https://127.0.0.1/cap/inventory</uri></map></llsd>";
    if(parse_seed_llsd(unsafe_inventory_seed,inv_event,&inv_mesh,nullptr,nullptr,
                       &inv_desc,&inv_items)||!inv_desc.empty())return 45;
    // The failure category is URL-free and remains safe to share.
    // EventQueue may still be granted even when the mesh URL is bad.
    const auto bad_scheme=
        "<llsd><map><key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/events</string>"
        "<key>ViewerAsset</key><uri>http://cdn.public-assets.org/m</uri>"
        "</map></llsd>";
    if(!parse_seed_llsd(bad_scheme,cap,&mesh_cap,&parse_report)||
       !mesh_cap.empty()||
       parse_report.mesh_issue!=L"non-HTTPS scheme")return 35;
    const auto bad_host=
        "<llsd><map><key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/events</string>"
        "<key>ViewerAsset</key><uri>https://127.0.0.1/mesh</uri>"
        "</map></llsd>";
    if(!parse_seed_llsd(bad_host,cap,&mesh_cap,&parse_report)||
       !mesh_cap.empty()||
       parse_report.mesh_issue!=L"hostname not public")return 36;
    const auto wrong_type=
        "<llsd><map><key>EventQueueGet</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/events</string>"
        "<key>ViewerAsset</key><undef/></map></llsd>";
    if(!parse_seed_llsd(wrong_type,cap,&mesh_cap,&parse_report)||
       !mesh_cap.empty()||
       parse_report.mesh_issue!=L"unsupported LLSD grant type")return 37;
    // A real trusted LLSD TeleportFailed body has an Info/Reason field.
    // Display only sanitized text. Every other event remains unsupported.
    const auto failed=
        "<llsd><map><key>id</key><integer>1004</integer>"
        "<key>events</key><array><map>"
        "<key>message</key><string>TeleportFailed</string>"
        "<key>body</key><map><key>Info</key><array><map>"
        "<key>Reason</key><string>Cannot enter this region.</string>"
        "</map></array></map></map></array></map></llsd>";
    const auto notice=observe_event_queue_response(failed);
    if(!notice.structurally_valid||!notice.safe_to_ack||
       notice.event_count!=1U||notice.handled_events!=1U||
       notice.unsupported_events!=0U||
       notice.notifications.size()!=1U||
       notice.notifications[0]!="Teleport failed: Cannot enter this region."||
       notice.next_ack.token!="1004")return 41;
    // A bearer URL must never leak through reason text into the UI.
    const auto url_failed=
        "<llsd><map><key>id</key><integer>1005</integer>"
        "<key>events</key><array><map>"
        "<key>message</key><string>TeleportFailed</string>"
        "<key>body</key><map><key>Reason</key>"
        "<string>https://simhost-123.agni.secondlife.io/cap/secret</string>"
        "</map></map></array></map></llsd>";
    const auto redacted=observe_event_queue_response(url_failed);
    if(!redacted.safe_to_ack||redacted.handled_events!=1U||
       redacted.notifications.size()!=1U||
       redacted.notifications[0]!="Teleport failed")return 42;
    const auto crossing=
        "<llsd><map><key>id</key><integer>1006</integer>"
        "<key>events</key><array><map>"
        "<key>message</key><string>TeleportFinish</string>"
        "<key>body</key><map><key>Info</key><array/></map>"
        "</map></array></map></llsd>";
    const auto unfinished=observe_event_queue_response(crossing);
    if(!unfinished.safe_to_ack||unfinished.handled_events!=0U||
       unfinished.unsupported_events!=1U||
       !unfinished.notifications.empty())return 43;
    // Do not treat malformed event elements as recognized actions.
    const auto invalid_action=
        "<llsd><map><key>id</key><integer>1007</integer>"
        "<key>events</key><array><map>"
        "<key>message</key><string>TeleportFailed!</string>"
        "<key>body</key><map><key>Reason</key><string>Oops</string></map>"
        "</map></array></map></llsd>";
    const auto unsafe_action=observe_event_queue_response(invalid_action);
    if(!unsafe_action.safe_to_ack||unsafe_action.handled_events!=0U||
       unsafe_action.unsupported_events!=1U)return 44;
    // Official ViewerAsset handles both mesh_id and texture_id. Mesh-only
    // GetMesh2 does NOT authorize a texture fetch; avoid arbitrary hosts.
    std::wstring texture_cap;
    const auto texture_seed=
        "<llsd><map><key>ViewerAsset</key>"
        "<uri>https://simhost-123.agni.secondlife.io/cap/assets</uri>"
        "<key>GetMesh2</key>"
        "<uri>https://simhost-123.agni.secondlife.io/cap/mesh</uri>"
        "</map></llsd>";
    if(!parse_seed_llsd(texture_seed,cap,&mesh_cap,nullptr,&texture_cap)||
       texture_cap!=L"https://simhost-123.agni.secondlife.io/cap/assets"||
       mesh_cap!=texture_cap)return 45;
    if(!parse_seed_llsd(alternate,cap,&mesh_cap,nullptr,&texture_cap)||
       !texture_cap.empty()||
       mesh_cap!=L"https://simhost-123.agni.secondlife.io/cap/new")
       return 46;
    if(!parse_seed_llsd(optional_bad,cap,&mesh_cap,nullptr,&texture_cap)||
       !texture_cap.empty())return 47;
    // A dropped CAPS seed used to leave KAMI at CAP NO indefinitely.
    // Retry only a few transient failures; never repeat rejected or
    // malformed requests or continue using the bearer URL forever.
    if(!retry_seed_response(0U,0U)||
       !retry_seed_response(429U,1U)||
       !retry_seed_response(503U,2U)||
       retry_seed_response(503U,3U)||
       retry_seed_response(401U,0U)||
       retry_seed_response(403U,0U)||
       retry_seed_response(404U,0U)||
       retry_seed_response(200U,0U))return 48;
    return 0;
}
}
int main(){
    const auto hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(hr))return 40;
    const auto result=exercise();
    CoUninitialize();
    return result;
}
