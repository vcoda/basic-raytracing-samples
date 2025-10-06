#include <sstream>
#include "application.h"
#include "magma/magma.h"

void onError(const std::string& message, const std::string& caption)
{
    std::cerr << message << std::endl;
#ifdef _WIN32
    MessageBoxA(NULL, message.c_str(), caption.c_str(), MB_ICONHAND);
#endif
}

std::string format(const char *what,
    const magma::exception::source_location& where)
{
    std::ostringstream oss;
    oss << what;
#ifdef MAGMA_DEBUG
    if (where.file_name())
        oss << std::endl << std::endl
        << where.file_name() << ", line " << where.line();
#else
    MAGMA_UNUSED(where);
#endif // MAGMA_DEBUG
    return oss.str();
}

template<class Error>
std::string formatError(const Error& error)
{
    std::ostringstream oss;
    oss << error.result() << std::endl
        << error.what() << std::endl << std::endl
        << error.description();
#ifdef MAGMA_DEBUG
    if (error.where().file_name())
        oss << std::endl << std::endl
        << error.where().file_name() << ", line " << error.where().line();
#endif // MAGMA_DEBUG
    return oss.str();
}

#ifdef MAGMA_NO_EXCEPTIONS
void runApp(const AppEntry& entry)
{
    magma::exception::setExceptionHandler(
        [](const char *what, const magma::exception::source_location& where)
        {
            std::string message = format(what, where);
            onError(message, "Magma");
            abort();
        });
    magma::exception::setErrorHandler(
        [](VkResult error, const char *what, const magma::exception::source_location& where)
        {
            std::string message = formatError(error, what, where);
            onError(message, "Vulkan");
            abort();
        });
    std::unique_ptr<IApplication> app = appFactory(entry);
    app->show();
    app->run();
}
#else
void runAppWithExceptionHandling(const AppEntry& entry) try
{
    std::unique_ptr<IApplication> app = appFactory(entry);
    app->show();
    app->run();
}
catch (const magma::exception::Error& exc)
{
    std::string message = formatError(exc);
    onError(message, "Vulkan");
}
catch (const magma::exception::ReflectionError& exc)
{
    std::string message = formatError(exc);
    onError(message, "SPIRV-Reflect");
}
catch (const magma::exception::Exception& exc)
{
    std::string message = format(exc.what(), exc.where());
    onError(message, "Magma");
}
catch (const std::exception& exc)
{
    std::ostringstream oss;
    oss << exc.what() << std::endl;
    onError(oss.str(), "Error");
}
#endif // MAGMA_NO_EXCEPTIONS

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
#else
int main(int argc, char *argv[])
#endif
{
#if defined(_MSC_VER) && defined(MAGMA_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    AppEntry entry;
#ifdef _WIN32
    entry.hInstance = hInstance;
    entry.hPrevInstance = hPrevInstance;
    entry.lpCmdLine = pCmdLine;
    entry.nCmdShow = nCmdShow;
#else
    entry.argc = argc;
    entry.argv = argv;
#endif
#ifdef MAGMA_NO_EXCEPTIONS
    runApp(entry);
#else
    runAppWithExceptionHandling(entry);
#endif
    return 0;
}
