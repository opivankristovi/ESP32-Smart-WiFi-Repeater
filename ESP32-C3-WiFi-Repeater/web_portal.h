#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

// Owns the HTTP WebServer: the setup/landing page and all config form handlers,
// the captive-portal redirects, live-readings JSON, and factory reset.
namespace WebPortal {

void begin();   // register routes + start server
void handle();  // pump the web server (call from loop)

}  // namespace WebPortal

#endif  // WEB_PORTAL_H
