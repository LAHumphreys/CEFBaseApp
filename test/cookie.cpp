#include <CefTestApp.h>
#include <CefTests.h>
#include <CefBaseThread.h>
#include <gtest/gtest.h>
#include <OSTools.h>

#include <include/cef_cookie.h>
#include <CefBaseCookieMgr.h>
#include <CefTestJSBaseTest.h>

CefBaseApp* app = nullptr;

class EnableFileCookies: public CefBrowserProcessHandler {
public:
    void OnContextInitialized() {
        // For tests we are using file URLs, and wish to support cookie access...
        CefCookieManager::GetGlobalManager(nullptr)->SetSupportedSchemes({"file"}, true, nullptr);
    }

    IMPLEMENT_REFCOUNTING(EnableFileCookies);
};

class GetAllCookies: public CefBaseIPCExec::IIPC_TriggeredAsyncFunction {
public:
    static constexpr const char* NAME = "GetAllCookies";

    virtual void Execute(
            CefRefPtr<CefBrowser> browser,
            std::unique_ptr<CefBaseIPCExec::IIPC_AsyncResult> result,
            CefBaseApp& app,
            std::string args) override
    {
        CefBaseCookies mgr(CefCookieManager::GetGlobalManager(nullptr));

        std::string url = args;

        if (url.empty()) {
            url = browser->GetMainFrame()->GetURL();
        }

        auto cookieString = std::make_shared<std::string>();
        std::shared_ptr<CefBaseIPCExec::IIPC_AsyncResult> sharedResult(result.release());

        mgr.ForEachCookie(
                url,
                [=] (const std::string& name,
                     const std::string& value,
                     const CefBaseCookies::RemainingCookies& status)
        {
            if (status != CefBaseCookies::RemainingCookies::NO_COOKIES) {
                cookieString->append(name);
                cookieString->append("=");
                cookieString->append(value);
            }

            if (status == CefBaseCookies::RemainingCookies::NO_MORE_COOKIES ||
                status == CefBaseCookies::RemainingCookies ::NO_COOKIES)
            {
                sharedResult->Dispatch(*cookieString);
            } else {
                cookieString->append("; ");
            }
        });
    }
};

class GetCookieMap: public CefBaseIPCExec::IIPC_TriggeredAsyncFunction {
public:
    static constexpr const char* NAME = "GetAllCookies_FromMap";

    virtual void Execute(
            CefRefPtr<CefBrowser> browser,
            std::unique_ptr<CefBaseIPCExec::IIPC_AsyncResult> result,
            CefBaseApp& app,
            std::string args) override
    {
        //CefBaseCookies mgr(CefCookieManager::GetGlobalManager(nullptr));
        auto mgr = app.Client().GlobalCookieJar();

        std::string url = args;

        if (url.empty()) {
            url = browser->GetMainFrame()->GetURL();
        }

        auto cookieString = std::make_shared<std::string>();
        std::shared_ptr<CefBaseIPCExec::IIPC_AsyncResult> sharedResult(result.release());

        mgr->GetCookieMap(url, [=] (std::shared_ptr<const CefBaseCookies::CookieNameValueMap> cookies) -> void {
            for (auto it = cookies->begin(); it != cookies->end(); ++it) {
                const std::string& name = it->first;
                const std::string& value = it->second;

                if (it != cookies->begin()) {
                    cookieString->append("; ");
                }

                cookieString->append(name);
                cookieString->append("=");
                cookieString->append(value);
            }
            sharedResult->Dispatch(*cookieString);
        });
    }
};

int main (int argc, char** argv) {
    const std::string rootPath = OS::Dirname(OS::GetExe());
    const std::string url = "file://" + rootPath + "/index.html";

    CefRefPtr<DummyCefApp> testApp(new DummyCefApp(argc, argv, url));
    app = testApp;

    testApp->Browser().InstallHandler(std::make_shared<EnableFileCookies>());
    testApp->IPC().Install(GetAllCookies::NAME, std::make_shared<GetAllCookies>());
    testApp->IPC().Install(GetCookieMap::NAME, std::make_shared<GetCookieMap>());

    DummyCefApp::RunTestsAndExit(testApp);
}

class JSTest: public JSTestBase {
public:
    CefRefPtr<CefV8Context> TestContext() {
        return DummyCefApp::GetTestContext();
    }

    CefRefPtr<CefBrowser> TestBrowser() {
        return DummyCefApp::GetTestBrowser();
    }

    CefBaseApp& App() {
        return *app;
    }

    std::string GetCookieString(const std::string& url = "") {
        std::promise<std::string> result;
        auto futureResult = result.get_future();
        App().IPC().Execute(
            PID_BROWSER,
            TestBrowser(),
            GetAllCookies::NAME, url, [&] (std::string cookies) -> void
            {
                result.set_value(std::move(cookies));
            });
        return futureResult.get();
    }

    std::string GetCookieMapString() {
        std::promise<std::string> result;
        auto futureResult = result.get_future();
        App().IPC().Execute(PID_BROWSER, TestBrowser(), GetCookieMap::NAME, "", [&] (std::string cookies) -> void {
            result.set_value(std::move(cookies));
        });
        return futureResult.get();
    }


    void SetUp() {
        ClearCookies();
    }

};

TEST_F(JSTest, JS_COOKIE) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie
    )JS";
    ExecuteCleanJS(code, "username=Test.User");
}

TEST_F(JSTest, JS_DELETE_COOKIE) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie.toString()
    )JS";
    ExecuteCleanJS(code, "username=Test.User");

    code = R"JS(
        (function () {
            let cookies = document.cookie.split(';');
            for (let i = 0; i < cookies.length; i +=1) {
                let cookie = cookies[i];
                document.cookie = cookie + '=; expires=Thu, 01 Jan 1970 00:00:01 GMT';
            }
        })();

        document.cookie.toString()
    )JS";
    ExecuteCleanJS(code, "");
}

TEST_F(JSTest, MGR_COOKIE) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie
    )JS";
    std::string expected = "username=Test.User";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieString(), expected);
}

TEST_F(JSTest, MGR_COOKIE_MAP) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie
    )JS";
    std::string expected = "username=Test.User";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieMapString(), expected);
}

TEST_F(JSTest, MGR_COOKIE_INVALID_URL) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie
    )JS";
    std::string expected = "username=Test.User";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieString("not a url"), "");
}

TEST_F(JSTest, DIFFERENT_URL) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie
    )JS";
    std::string expected = "username=Test.User";
    ExecuteCleanJS(code, expected);

    std::string url = "https://www.google.co.uk";

    ASSERT_EQ(GetCookieString(url), "");
}

TEST_F(JSTest, MGR_MULTIPLE_COOKIE) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie = 'username2=Test.User2';
        document.cookie
    )JS";
    std::string expected = "username=Test.User; username2=Test.User2";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieString(), expected);
}

TEST_F(JSTest, MGR_MULTIPLE_COOKIE_MAP) {
    std::string code = R"JS(
        document.cookie = 'username=Test.User';
        document.cookie = 'username2=Test.User2';
        document.cookie
    )JS";
    std::string expected = "username=Test.User; username2=Test.User2";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieMapString(), expected);
}

TEST_F(JSTest, JS_NO_COOKIE) {
    std::string code = R"JS(
        document.cookie
    )JS";
    ExecuteCleanJS(code, "");
}

TEST_F(JSTest, MGR_NO_COOKIE) {
    std::string code = R"JS(
        document.cookie
    )JS";
    std::string expected = "";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieString(), expected);
}

TEST_F(JSTest, MGR_NO_COOKIE_MAP) {
    std::string code = R"JS(
        document.cookie
    )JS";
    std::string expected = "";
    ExecuteCleanJS(code, expected);

    ASSERT_EQ(GetCookieMapString(), expected);
}
