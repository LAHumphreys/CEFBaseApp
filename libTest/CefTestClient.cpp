#include <CefTestApp.h>
#include "CefTestClient.h"
#include "logger.h"
#include "CefTestAppHandlers.h"

/**********************************************************************
*                         Client
**********************************************************************/


DummyCefClient::DummyCefClient()
   : browser(nullptr)
{
}

void DummyCefClient::OnAfterCreated(CefRefPtr<CefBrowser> _browser) {
    if (  browser != nullptr) {
        LOG_FROM(LOG_ERROR,"DummyCefClient::OnAfterCreated","Second Browser!");
        abort();
    }
    browser = _browser;
    DummyCefApp::SetTestBrowser(browser);
}


void DummyCefClient::OnBeforeClose(CefRefPtr<CefBrowser> _browser) {
    browser = nullptr;
}

bool DummyCefClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message)
{
    bool handled = true;
    std::string name = message->GetName().ToString();

    if (name == "EXIT") {
        DummyCefAppHandlers::Exit();
    } else if (name == "ABORT") {
        DummyCefAppHandlers::Abort();
    }

    return handled;
}

void DummyCefClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect{1, 1, 100, 100};
}