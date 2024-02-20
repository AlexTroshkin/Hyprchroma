#include "WindowInverter.h"

void WindowInverter::SetBackground(GLfloat r, GLfloat g, GLfloat b)
{
    bkgR = r;
    bkgG = g;
    bkgB = b;
}

void WindowInverter::OnRenderWindowPre()
{
    bool shouldInvert =
        std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), g_pHyprOpenGL->m_pCurrentWindow)
            != m_InvertedWindows.end() ||
        std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), g_pHyprOpenGL->m_pCurrentWindow)
            != m_ManuallyInvertedWindows.end();

    if (shouldInvert)
    {
        glUseProgram(m_Shaders.RGBA.program);
        glUniform3f(m_Shaders.BKGA, bkgR, bkgG, bkgB);
        glUseProgram(m_Shaders.RGBX.program);
        glUniform3f(m_Shaders.BKGX, bkgR, bkgG, bkgB);
        glUseProgram(m_Shaders.EXT.program);
        glUniform3f(m_Shaders.BKGE, bkgR, bkgG, bkgB);
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = true;
    }
}

void WindowInverter::OnRenderWindowPost()
{
    if (m_ShadersSwapped)
    {
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = false;
    }
}

void WindowInverter::OnWindowClose(CWindow* window)
{
    auto windowIt = std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), window);
    if (windowIt != m_InvertedWindows.end())
    {
        std::swap(*windowIt, *(m_InvertedWindows.end() - 1));
        m_InvertedWindows.pop_back();
    }

    windowIt = std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), window);
    if (windowIt != m_ManuallyInvertedWindows.end())
    {
        std::swap(*windowIt, *(m_ManuallyInvertedWindows.end() - 1));
        m_ManuallyInvertedWindows.pop_back();
    }
}


void WindowInverter::SetRules(std::vector<SWindowRule>&& rules)
{
    m_InvertWindowRules = rules;
    Reload();
}


void WindowInverter::Init()
{
    m_Shaders.Init();
}

void WindowInverter::Unload()
{
    if (m_ShadersSwapped)
    {
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = false;
    }

    m_Shaders.Destroy();
}

void WindowInverter::InvertIfMatches(CWindow* window)
{
    std::string              title      = g_pXWaylandManager->getTitle(window);
    std::string              appidclass = g_pXWaylandManager->getAppIDClass(window);

    bool shouldInvert = false;
    for (const auto& rule : m_InvertWindowRules)
    {
        try
        {
            if (rule.szClass != "") {
                std::regex RULECHECK(rule.szClass);

                if (!std::regex_search(appidclass, RULECHECK))
                    continue;
            }

            if (rule.szTitle != "") {
                std::regex RULECHECK(rule.szTitle);

                if (!std::regex_search(title, RULECHECK))
                    continue;
            }

            if (rule.bX11 != -1) {
                if (window->m_bIsX11 != rule.bX11)
                    continue;
            }

            if (rule.bFloating != -1) {
                if (window->m_bIsFloating != rule.bFloating)
                    continue;
            }

            if (rule.bFullscreen != -1) {
                if (window->m_bIsFullscreen != rule.bFullscreen)
                    continue;
            }

            if (rule.bPinned != -1) {
                if (window->m_bPinned != rule.bPinned)
                    continue;
            }

            if (!rule.szWorkspace.empty()) {
                const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(window->m_iWorkspaceID);

                if (!PWORKSPACE)
                    continue;

                if (rule.szWorkspace.find("name:") == 0) {
                    if (PWORKSPACE->m_szName != rule.szWorkspace.substr(5))
                        continue;
                } else {
                    // number
                    if (!isNumber(rule.szWorkspace))
                        throw std::runtime_error("szWorkspace not name: or number");

                    const int64_t ID = std::stoll(rule.szWorkspace);

                    if (PWORKSPACE->m_iID != ID)
                        continue;
                }
            }
        }
        catch (std::exception& e)
        {
            Debug::log(ERR, "Regex error at %s (%s)", rule.szValue.c_str(), e.what());
            continue;
        }

        shouldInvert = true;
        break;
    }

    auto windowIt = std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), window);
    if (shouldInvert != (windowIt != m_InvertedWindows.end()))
    {
        if (shouldInvert)
            m_InvertedWindows.push_back(window);
        else
        {
            std::swap(*windowIt, *(m_InvertedWindows.end() - 1));
            m_InvertedWindows.pop_back();
        }

        g_pHyprRenderer->damageWindow(window);
    }
}


void WindowInverter::ToggleInvert(CWindow* window)
{
    if (!window)
        return;

    auto windowIt = std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), window);
    if (windowIt == m_ManuallyInvertedWindows.end())
        m_ManuallyInvertedWindows.push_back(window);
    else
    {
        std::swap(*windowIt, *(m_ManuallyInvertedWindows.end() - 1));
        m_ManuallyInvertedWindows.pop_back();
    }

    g_pHyprRenderer->damageWindow(window);
}


void WindowInverter::Reload()
{
    m_InvertedWindows = {};

    for (const auto& window : g_pCompositor->m_vWindows)
        InvertIfMatches(window.get());
}
