#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <richole.h>
#include <gdiplus.h>
#include <tom.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "plugin2.h"
#include "resource.h"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdiplus.lib")

namespace {

    constexpr wchar_t kWindowClass[] = L"OfficeStyleRichTextToolbar";
    constexpr wchar_t kWindowName[] = L"Office-Style RichText";
    constexpr int kBoldButton = 1001;
    constexpr int kItalicButton = 1002;
    constexpr int kFontCombo = 1003;
    constexpr int kApplyFontButton = 1004;
    constexpr int kSizeCombo = 1005;
    constexpr int kApplySizeButton = 1006;
    constexpr int kColorButton = 1007;
    constexpr int kLoadButton = 1008;
    constexpr int kApplyButton = 1009;
    constexpr int kTextEyedropperButton = 1010;
    constexpr int kBackgroundButton = 1011;
    constexpr int kBackgroundEyedropperButton = 1012;
    constexpr int kOutlineColorButton = 1014;
    constexpr int kOutlineEyedropperButton = 1015;
    constexpr int kOutlinePreviewButton = 1017;
    constexpr UINT kPopulateFontsMessage = WM_APP + 42;
constexpr COLORREF kButtonColor = RGB(55, 55, 55);
constexpr COLORREF kButtonPressedColor = RGB(78, 78, 78);
constexpr COLORREF kButtonTextColor = RGB(240, 240, 240);
constexpr COLORREF kControlBorderColor = RGB(170, 170, 170);

    enum class ColorPickTarget { Text, Outline, Background };

    struct TextStyle {
        std::wstring font = L"Yu Gothic UI";
        int size = 32;
        COLORREF color = RGB(255, 255, 255);
        COLORREF outlineColor = RGB(0, 0, 0);
        int decoration = 0;
        bool bold = false;
        bool italic = false;
    };

    HMODULE g_module = nullptr;
    HMODULE g_richEditModule = nullptr;
    HICON g_eyedropperIcon = nullptr;
HBRUSH g_panelBrush = nullptr;
HBRUSH g_buttonBrush = nullptr;
HBRUSH g_buttonPressedBrush = nullptr;
HBRUSH g_borderBrush = nullptr;
    EDIT_HANDLE* g_editHandle = nullptr;
    HWND g_toolbarWindow = nullptr;
    HWND g_editor = nullptr;
    HWND g_fontCombo = nullptr;
    HWND g_sizeCombo = nullptr;
    HWND g_statusLabel = nullptr;
    int g_editorLeft = 0;
    int g_editorTop = 0;
    int g_editorRightMargin = 8;
    int g_editorBottomMargin = 8;
    int g_statusLeft = 0;
    int g_statusTop = 0;
    int g_statusRightMargin = 8;
    int g_statusHeight = 20;
    TextStyle g_baseStyle;
    COLORREF g_customColors[16] = {};
    COLORREF g_outlineColor = RGB(0, 0, 0);
    std::vector<COLORREF> g_outlineColors;
    std::wstring g_editorTextSnapshot;
    COLORREF g_editorBackground = RGB(0, 0, 0);
    bool g_pickingColor = false;
    bool g_fontsLoaded = false;
    ColorPickTarget g_pickTarget = ColorPickTarget::Text;
    ULONG_PTR g_gdiplusToken = 0;
    bool g_drawingOutlinePreview = false;
    bool g_outlinePreviewEnabled = true;

    void SetStatus(const wchar_t* message);
    std::wstring ReadEditorText();
    std::wstring Utf8ToWide(const char* text);
    std::string WideToUtf8(const wchar_t* text, int length);
    bool ParseMarkup(const std::wstring& raw, std::wstring& visible,
        std::vector<TextStyle>& styles);
    void SyncOutlineColors();
    void DrawOutlinePreview(HWND editor);
    void SetEditorDocument(const std::wstring& visible,
        const std::vector<TextStyle>& styles);
    std::string SerializeEditor();

    void SetStatus(const wchar_t* message) {
        if (g_statusLabel && IsWindow(g_statusLabel)) {
            SetWindowTextW(g_statusLabel, message);
        }
    }

    void DrawOutlinePreview(HWND editor) {
        if (!g_outlinePreviewEnabled || editor != g_editor || g_outlineColors.empty() ||
            g_drawingOutlinePreview) {
            return;
        }

        const std::wstring text = ReadEditorText();
        if (text.empty()) {
            return;
        }

        RECT client = {};
        GetClientRect(editor, &client);
        POINT corners[] = {
            { client.left + 1, client.top + 1 },
            { client.right - 2, client.top + 1 },
            { client.left + 1, client.bottom - 2 },
            { client.right - 2, client.bottom - 2 }
        };
        LONG first = static_cast<LONG>(text.size());
        LONG last = 0;
        for (const POINT& point : corners) {
            const LRESULT hit = SendMessageW(editor, EM_CHARFROMPOS, 0,
                reinterpret_cast<LPARAM>(&point));
            const LONG index = static_cast<LONG>(LOWORD(hit));
            first = (std::min)(first, index);
            last = (std::max)(last, index);
        }
        first = (std::max)(0L, first - 2);
        last = (std::min)(static_cast<LONG>(text.size()), last + 3);
        if (last <= first || last - first > 4096) {
            return;
        }

        CHARRANGE originalSelection = {};
        SendMessageW(editor, EM_EXGETSEL, 0,
            reinterpret_cast<LPARAM>(&originalSelection));
        POINT originalScroll = {};
        SendMessageW(editor, EM_GETSCROLLPOS, 0,
            reinterpret_cast<LPARAM>(&originalScroll));
        g_drawingOutlinePreview = true;

        HDC dc = GetDC(editor);
        if (!dc) {
            g_drawingOutlinePreview = false;
            return;
        }
        Gdiplus::Graphics graphics(dc);
        graphics.SetPageUnit(Gdiplus::UnitPixel);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        LONG zoomNumerator = 0;
        LONG zoomDenominator = 0;
        SendMessageW(editor, EM_GETZOOM,
            reinterpret_cast<WPARAM>(&zoomNumerator),
            reinterpret_cast<LPARAM>(&zoomDenominator));
        Gdiplus::REAL zoomScale = 1.0f;
        if (zoomNumerator > 0 && zoomDenominator > 0) {
            zoomScale = static_cast<Gdiplus::REAL>(zoomNumerator) /
                static_cast<Gdiplus::REAL>(zoomDenominator);
        }
        const Gdiplus::REAL dpiScale =
            static_cast<Gdiplus::REAL>(GetDeviceCaps(dc, LOGPIXELSY)) / 72.0f * zoomScale;

        IRichEditOle* richEditOle = nullptr;
        ITextDocument* textDocument = nullptr;
        if (SendMessageW(editor, EM_GETOLEINTERFACE, 0,
            reinterpret_cast<LPARAM>(&richEditOle)) && richEditOle) {
            richEditOle->QueryInterface(__uuidof(ITextDocument),
                reinterpret_cast<void**>(&textDocument));
        }

        for (LONG index = first; index < last; ++index) {
            if (index >= static_cast<LONG>(g_outlineColors.size()) ||
                text[static_cast<size_t>(index)] == L'\r' ||
                text[static_cast<size_t>(index)] == L'\n') {
                continue;
            }

            size_t charLength = 1;
            if (text[static_cast<size_t>(index)] >= 0xD800 &&
                text[static_cast<size_t>(index)] <= 0xDBFF && index + 1 < last &&
                text[static_cast<size_t>(index + 1)] >= 0xDC00 &&
                text[static_cast<size_t>(index + 1)] <= 0xDFFF) {
                charLength = 2;
            }

            CHARRANGE range{ index, index + static_cast<LONG>(charLength) };
            SendMessageW(editor, EM_EXSETSEL, 0,
                reinterpret_cast<LPARAM>(&range));
            CHARFORMAT2W format = {};
            format.cbSize = sizeof(format);
            SendMessageW(editor, EM_GETCHARFORMAT, SCF_SELECTION,
                reinterpret_cast<LPARAM>(&format));
            SendMessageW(editor, EM_SETSCROLLPOS, 0,
                reinterpret_cast<LPARAM>(&originalScroll));

            LONG baselineX = 0;
            LONG baselineY = 0;
            bool hasBaseline = false;
            ITextRange* textRange = nullptr;
            if (textDocument && SUCCEEDED(textDocument->Range(
                index, index + static_cast<LONG>(charLength), &textRange)) && textRange) {
                hasBaseline = textRange->GetPoint(
                    tomStart | tomClientCoord | tomAllowOffClient |
                        TA_BASELINE | TA_LEFT,
                    &baselineX, &baselineY) == S_OK;
                textRange->Release();
            }
            if (!hasBaseline) {
                continue;
            }
            const int x = baselineX;
            const int y = baselineY;

            wchar_t face[LF_FACESIZE] = {};
            if ((format.dwMask & CFM_FACE) && format.szFaceName[0]) {
                wcscpy_s(face, format.szFaceName);
            }
            else {
                wcscpy_s(face, L"Yu Gothic UI");
            }
            Gdiplus::FontFamily family(face);
            const INT fontStyle = ((format.dwEffects & CFE_BOLD) ? Gdiplus::FontStyleBold : 0) |
                ((format.dwEffects & CFE_ITALIC) ? Gdiplus::FontStyleItalic : 0);
            const LONG height = (format.dwMask & CFM_SIZE)
                ? format.yHeight : g_baseStyle.size * 20;
            const Gdiplus::REAL fontSize =
                static_cast<Gdiplus::REAL>(height) / 20.0f * dpiScale;
            if (fontSize <= 0.0f) {
                continue;
            }

            Gdiplus::GraphicsPath path;
            Gdiplus::StringFormat stringFormat(Gdiplus::StringFormat::GenericTypographic());
            stringFormat.SetFormatFlags(stringFormat.GetFormatFlags() |
                Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip);
            const std::wstring glyph = text.substr(static_cast<size_t>(index), charLength);
            const UINT emHeight = family.GetEmHeight(fontStyle);
            const UINT ascent = family.GetCellAscent(fontStyle);
            const Gdiplus::REAL baselineOffset = emHeight
                ? fontSize * static_cast<Gdiplus::REAL>(ascent) /
                    static_cast<Gdiplus::REAL>(emHeight)
                : fontSize;
            if (!hasBaseline) {
                baselineY += static_cast<LONG>(baselineOffset);
            }
            const Gdiplus::RectF layout(static_cast<Gdiplus::REAL>(x),
                static_cast<Gdiplus::REAL>(hasBaseline ? y : baselineY) - baselineOffset,
                fontSize * 2.5f, fontSize * 2.5f);
            if (path.AddString(glyph.c_str(), static_cast<INT>(glyph.size()),
                &family, fontStyle, fontSize, layout, &stringFormat) != Gdiplus::Ok) {
                continue;
            }

            Gdiplus::Pen pen(Gdiplus::Color(255,
                GetRValue(g_outlineColors[static_cast<size_t>(index)]),
                GetGValue(g_outlineColors[static_cast<size_t>(index)]),
                GetBValue(g_outlineColors[static_cast<size_t>(index)])),
                dpiScale * 3.6f > 1.0f ? dpiScale * 3.6f : 1.0f);
            pen.SetLineJoin(Gdiplus::LineJoinRound);
            graphics.DrawPath(&pen, &path);
        }

        SendMessageW(editor, EM_EXSETSEL, 0,
            reinterpret_cast<LPARAM>(&originalSelection));
        SendMessageW(editor, EM_SETSCROLLPOS, 0,
            reinterpret_cast<LPARAM>(&originalScroll));
        if (textDocument) {
            textDocument->Release();
        }
        if (richEditOle) {
            richEditOle->Release();
        }
        ReleaseDC(editor, dc);
        g_drawingOutlinePreview = false;
    }

    std::wstring Utf8ToWide(const char* text) {
        if (!text || !*text) {
            return {};
        }
        const int inputLength = static_cast<int>(strlen(text));
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text, inputLength, nullptr, 0);
        if (length <= 0) {
            return {};
        }
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            text, inputLength, result.data(), length);
        return result;
    }

    std::string WideToUtf8(const wchar_t* text, int length) {
        if (!text || length <= 0) {
            return {};
        }
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text, length, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) {
            return {};
        }
        std::string result(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text, length, result.data(), bytes, nullptr, nullptr);
        return result;
    }

    bool ParseHexColor(const std::wstring& value, COLORREF& color) {
        std::wstring digits = value;
        if (!digits.empty() && digits[0] == L'#') {
            digits.erase(0, 1);
        }
        if (digits.size() < 6) {
            return false;
        }
        const std::wstring six = digits.substr(0, 6);
        wchar_t* end = nullptr;
        const unsigned long rgb = std::wcstoul(six.c_str(), &end, 16);
        if (!end || *end != L'\0') {
            return false;
        }
        color = RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
        return true;
    }

    std::wstring ColorToHex(COLORREF color) {
        wchar_t value[7] = {};
        swprintf_s(value, L"%02X%02X%02X",
            GetRValue(color), GetGValue(color), GetBValue(color));
        return value;
    }

    OBJECT_HANDLE FindSelectedTextObject(EDIT_SECTION* edit) {
        if (!edit) {
            return nullptr;
        }

        if (edit->get_focus_object) {
            OBJECT_HANDLE focused = edit->get_focus_object();
            if (focused && edit->get_object_item_value &&
                edit->get_object_item_value(focused, L"テキスト", L"テキスト")) {
                return focused;
            }
        }

        if (!edit->get_selected_object_num || !edit->get_selected_object) {
            return nullptr;
        }
        const int count = edit->get_selected_object_num();
        for (int i = 0; i < count; ++i) {
            OBJECT_HANDLE object = edit->get_selected_object(i);
            if (object && edit->get_object_item_value &&
                edit->get_object_item_value(object, L"テキスト", L"テキスト")) {
                return object;
            }
        }
        return nullptr;
    }

    int ParseSize(const char* value, int fallback) {
        if (!value || !*value) {
            return fallback;
        }
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        return (end != value && parsed >= 1 && parsed <= 1000)
            ? static_cast<int>(parsed) : fallback;
    }

    int ParseDecoration(const char* value, int fallback) {
        if (!value || !*value) {
            return fallback;
        }
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        return (end != value && parsed >= 0 && parsed <= 6)
            ? static_cast<int>(parsed) : fallback;
    }

    struct ReadTextRequest {
        std::string text;
        std::string font;
        std::string size;
        std::string color;
        std::string outlineColor;
        std::string decoration;
        bool found = false;
    };

    void ReadItem(EDIT_SECTION* edit, OBJECT_HANDLE object, LPCWSTR item,
        std::string& output) {
        if (!edit || !edit->get_object_item_value) {
            return;
        }
        LPCSTR value = edit->get_object_item_value(object, L"テキスト", item);
        if (value) {
            output = value;
        }
    }

    void LoadSelectedText() {
        if (!g_editHandle || !g_editHandle->call_read_section_param) {
            SetStatus(L"SDKから編集情報を取得できません。");
            return;
        }

        ReadTextRequest request;
        const bool read = g_editHandle->call_read_section_param(&request,
            [](void* parameter, EDIT_SECTION* edit) {
                auto* request = static_cast<ReadTextRequest*>(parameter);
                if (!request || !edit) {
                    return;
                }
                OBJECT_HANDLE object = FindSelectedTextObject(edit);
                if (!object) {
                    return;
                }
                ReadItem(edit, object, L"テキスト", request->text);
                ReadItem(edit, object, L"フォント", request->font);
                ReadItem(edit, object, L"サイズ", request->size);
                ReadItem(edit, object, L"文字色", request->color);
                ReadItem(edit, object, L"影・縁色", request->outlineColor);
                ReadItem(edit, object, L"文字装飾", request->decoration);
                request->found = true;
            });

        if (!read || !request.found) {
            SetStatus(L"テキストオブジェクトを選択してから「読み込み」を押してください。");
            return;
        }

        g_baseStyle = {};
        g_outlineColor = RGB(0, 0, 0);
        if (!request.font.empty()) {
            g_baseStyle.font = Utf8ToWide(request.font.c_str());
        }
        g_baseStyle.size = ParseSize(request.size.c_str(), 32);
        if (!request.color.empty()) {
            ParseHexColor(Utf8ToWide(request.color.c_str()), g_baseStyle.color);
        }
        if (!request.outlineColor.empty()) {
            ParseHexColor(Utf8ToWide(request.outlineColor.c_str()), g_outlineColor);
        }
        g_baseStyle.outlineColor = g_outlineColor;
        g_baseStyle.decoration = ParseDecoration(
            request.decoration.c_str(), 0);

        const std::wstring raw = Utf8ToWide(request.text.c_str());
        std::wstring visible;
        std::vector<TextStyle> styles;
        ParseMarkup(raw, visible, styles);
        SetEditorDocument(visible, styles);
        SetWindowTextW(g_fontCombo, g_baseStyle.font.c_str());
        wchar_t size[16] = {};
        swprintf_s(size, L"%d", g_baseStyle.size);
        SetWindowTextW(g_sizeCombo, size);
        SetStatus(L"選択中のテキストを読み込みました。装飾後に「反映」を押してください。");
    }

    struct WriteTextRequest {
        const std::string* value = nullptr;
        bool updated = false;
    };

    void ApplyTextToSelectedObject() {
        if (!g_editHandle || !g_editHandle->call_edit_section_param) {
            SetStatus(L"SDKから編集情報を更新できません。");
            return;
        }

        const std::string value = SerializeEditor();
        WriteTextRequest request{ &value, false };
        const bool applied = g_editHandle->call_edit_section_param(&request,
            [](void* parameter, EDIT_SECTION* edit) {
                auto* request = static_cast<WriteTextRequest*>(parameter);
                if (!request || !request->value || !edit ||
                    !edit->set_object_item_value) {
                    return;
                }
                OBJECT_HANDLE object = FindSelectedTextObject(edit);
                if (object) {
                    request->updated = edit->set_object_item_value(
                        object, L"テキスト", L"テキスト", request->value->c_str());
                }
            });

        if (!applied || !request.updated) {
            SetStatus(L"選択中のテキストオブジェクトへ反映できませんでした。");
            return;
        }
        SetStatus(L"テキストオブジェクトへ反映しました。");
    }

    bool SameStyle(const TextStyle& left, const TextStyle& right) {
        return left.font == right.font && left.size == right.size &&
            left.color == right.color && left.outlineColor == right.outlineColor &&
            left.decoration == right.decoration &&
            left.bold == right.bold && left.italic == right.italic;
    }

    void ApplyFormatToRange(LONG start, LONG end, const TextStyle& style) {
        if (!g_editor || end <= start) {
            return;
        }
        CHARRANGE range{ start, end };
        SendMessageW(g_editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
        format.dwEffects = (style.bold ? CFE_BOLD : 0) |
            (style.italic ? CFE_ITALIC : 0);
        format.yHeight = style.size * 20;
        format.crTextColor = style.color;
        wcsncpy_s(format.szFaceName, style.font.c_str(), _TRUNCATE);
        SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_SELECTION,
            reinterpret_cast<LPARAM>(&format));
    }

    void SetEditorDocument(const std::wstring& visible,
        const std::vector<TextStyle>& styles) {
        if (!g_editor) {
            return;
        }

        CHARFORMAT2W base = {};
        base.cbSize = sizeof(base);
        base.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD | CFM_ITALIC;
        base.dwEffects = 0;
        base.yHeight = g_baseStyle.size * 20;
        base.crTextColor = g_baseStyle.color;
        wcsncpy_s(base.szFaceName, g_baseStyle.font.c_str(), _TRUNCATE);
        SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_ALL,
            reinterpret_cast<LPARAM>(&base));
        g_outlineColors.clear();
        g_outlineColors.reserve(styles.size());
        for (const TextStyle& style : styles) {
            g_outlineColors.push_back(style.outlineColor);
        }
        g_outlineColors.resize(visible.size(), g_baseStyle.outlineColor);
        g_editorTextSnapshot = visible;
        SetWindowTextW(g_editor, visible.c_str());

        size_t start = 0;
        while (start < styles.size()) {
            size_t end = start + 1;
            while (end < styles.size() && SameStyle(styles[start], styles[end])) {
                ++end;
            }
            ApplyFormatToRange(static_cast<LONG>(start),
                static_cast<LONG>(end), styles[start]);
            start = end;
        }

        CHARRANGE endRange{ static_cast<LONG>(visible.size()),
            static_cast<LONG>(visible.size()) };
        SendMessageW(g_editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&endRange));
    }

    bool ParseMarkup(const std::wstring& raw, std::wstring& visible,
        std::vector<TextStyle>& styles) {
        TextStyle current = g_baseStyle;
        for (size_t i = 0; i < raw.size();) {
            if (raw[i] == L'<') {
                const size_t end = raw.find(L'>', i + 1);
                if (end != std::wstring::npos && end - i < 256) {
                    const std::wstring token = raw.substr(i + 1, end - i - 1);
                    bool recognized = false;

                    if (!token.empty() && token[0] == L'#') {
                        if (token.size() == 1) {
                            current.color = g_baseStyle.color;
                            current.outlineColor = g_baseStyle.outlineColor;
                            recognized = true;
                        }
                        else {
                            const size_t comma = token.find(L',');
                            const std::wstring color = token.substr(1,
                                comma == std::wstring::npos ? comma : comma - 1);
                            recognized = ParseHexColor(color, current.color);
                            if (recognized && comma != std::wstring::npos) {
                                const std::wstring outlineColor = token.substr(comma + 1);
                                if (outlineColor.empty()) {
                                    current.outlineColor = g_baseStyle.outlineColor;
                                }
                                else {
                                    recognized = ParseHexColor(outlineColor,
                                        current.outlineColor);
                                }
                            }
                        }
                    }
                    else if (token == L"@") {
                        current.font = g_baseStyle.font;
                        current.bold = g_baseStyle.bold;
                        current.italic = g_baseStyle.italic;
                        current.decoration = g_baseStyle.decoration;
                        recognized = true;
                    }
                    else if (token == L"@+B") {
                        current.bold = true;
                        recognized = true;
                    }
                    else if (token == L"@-B") {
                        current.bold = false;
                        recognized = true;
                    }
                    else if (token == L"@+I") {
                        current.italic = true;
                        recognized = true;
                    }
                    else if (token == L"@-I") {
                        current.italic = false;
                        recognized = true;
                    }
                    else if (token.size() > 1 && token[0] == L'@') {
                        const size_t comma = token.find(L',');
                        current.font = token.substr(1,
                            comma == std::wstring::npos ? comma : comma - 1);
                        current.bold = false;
                        current.italic = false;
                        current.decoration = g_baseStyle.decoration;
                        if (comma != std::wstring::npos) {
                            const std::wstring decoration = token.substr(comma + 1);
                            if (!decoration.empty() && decoration[0] >= L'0' &&
                                decoration[0] <= L'6') {
                                current.decoration = decoration[0] - L'0';
                            }
                            current.bold = decoration.find(L'B') != std::wstring::npos;
                            current.italic = decoration.find(L'I') != std::wstring::npos;
                        }
                        recognized = !current.font.empty();
                    }
                    else if (!token.empty() && token[0] == L's') {
                        if (token.size() == 1) {
                            current.size = g_baseStyle.size;
                            recognized = true;
                        }
                        else {
                            std::vector<std::wstring> fields;
                            size_t begin = 1;
                            while (begin <= token.size()) {
                                const size_t comma = token.find(L',', begin);
                                fields.push_back(token.substr(begin,
                                    comma == std::wstring::npos ? comma : comma - begin));
                                if (comma == std::wstring::npos) {
                                    break;
                                }
                                begin = comma + 1;
                            }
                            wchar_t* parsedEnd = nullptr;
                            const long size = std::wcstol(fields[0].c_str(), &parsedEnd, 10);
                            if (parsedEnd != fields[0].c_str() && size >= 1 && size <= 1000) {
                                current.size = static_cast<int>(size);
                                if (fields.size() > 1 && !fields[1].empty()) {
                                    current.font = fields[1];
                                }
                                if (fields.size() > 2) {
                                    const std::wstring decoration = fields[2];
                                    size_t styleStart = 0;
                                    if (!decoration.empty() &&
                                        decoration[0] >= L'0' && decoration[0] <= L'6') {
                                        current.decoration = decoration[0] - L'0';
                                        styleStart = 1;
                                    }
                                    current.bold = decoration.find(L'B', styleStart) != std::wstring::npos;
                                    current.italic = decoration.find(L'I', styleStart) != std::wstring::npos;
                                }
                                recognized = true;
                            }
                        }
                    }

                    if (recognized) {
                        i = end + 1;
                        continue;
                    }
                }
            }

            visible.push_back(raw[i]);
            styles.push_back(current);
            ++i;
        }
        return true;
    }

    std::wstring ReadEditorText() {
        const int length = GetWindowTextLengthW(g_editor);
        std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(g_editor, buffer.data(), length + 1);
        return std::wstring(buffer.data(), static_cast<size_t>(length));
    }

    void SyncOutlineColors() {
        if (!g_editor) {
            return;
        }

        const std::wstring currentText = ReadEditorText();
        if (g_outlineColors.size() != g_editorTextSnapshot.size()) {
            g_outlineColors.resize(g_editorTextSnapshot.size(),
                g_baseStyle.outlineColor);
        }

        size_t prefix = 0;
        const size_t commonLength = (std::min)(g_editorTextSnapshot.size(),
            currentText.size());
        while (prefix < commonLength &&
            g_editorTextSnapshot[prefix] == currentText[prefix]) {
            ++prefix;
        }

        size_t suffix = 0;
        while (suffix < g_editorTextSnapshot.size() - prefix &&
            suffix < currentText.size() - prefix &&
            g_editorTextSnapshot[g_editorTextSnapshot.size() - suffix - 1] ==
            currentText[currentText.size() - suffix - 1]) {
            ++suffix;
        }

        const size_t oldEnd = g_editorTextSnapshot.size() - suffix;
        const size_t newEnd = currentText.size() - suffix;
        const size_t removed = oldEnd - prefix;
        const size_t inserted = newEnd - prefix;
        COLORREF inheritedColor = g_baseStyle.outlineColor;
        if (removed > 0 && prefix < g_outlineColors.size()) {
            inheritedColor = g_outlineColors[prefix];
        }
        else if (prefix > 0 && prefix - 1 < g_outlineColors.size()) {
            inheritedColor = g_outlineColors[prefix - 1];
        }
        else if (!g_outlineColors.empty()) {
            inheritedColor = g_outlineColors.front();
        }

        g_outlineColors.erase(g_outlineColors.begin() + prefix,
            g_outlineColors.begin() + oldEnd);
        g_outlineColors.insert(g_outlineColors.begin() + prefix, inserted,
            inheritedColor);
        g_editorTextSnapshot = currentText;
    }

    TextStyle GetFormatAt(LONG position) {
        CHARRANGE range{ position, position + 1 };
        SendMessageW(g_editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        SendMessageW(g_editor, EM_GETCHARFORMAT, SCF_SELECTION,
            reinterpret_cast<LPARAM>(&format));

        TextStyle style = g_baseStyle;
        if (format.dwMask & CFM_FACE) {
            style.font = format.szFaceName;
        }
        if (format.dwMask & CFM_SIZE) {
            style.size = (format.yHeight + 10) / 20;
        }
        if (format.dwMask & CFM_COLOR) {
            style.color = format.crTextColor;
        }
        if (format.dwMask & CFM_BOLD) {
            style.bold = (format.dwEffects & CFE_BOLD) != 0;
        }
        if (format.dwMask & CFM_ITALIC) {
            style.italic = (format.dwEffects & CFE_ITALIC) != 0;
        }
        if (position >= 0 && static_cast<size_t>(position) < g_outlineColors.size()) {
            style.outlineColor = g_outlineColors[static_cast<size_t>(position)];
        }
        return style;
    }

    std::string SerializeEditor() {
        const std::wstring text = ReadEditorText();
        std::string output;
        TextStyle previous = g_baseStyle;
        size_t start = 0;

        while (start < text.size()) {
            const TextStyle style = GetFormatAt(static_cast<LONG>(start));
            size_t end = start + 1;
            while (end < text.size() &&
                SameStyle(style, GetFormatAt(static_cast<LONG>(end)))) {
                ++end;
            }

            if (style.size != previous.size) {
                if (style.size == g_baseStyle.size) {
                    output += "<s>";
                }
                else {
                    wchar_t sizeTag[32] = {};
                    swprintf_s(sizeTag, L"<s%d>", style.size);
                    output += WideToUtf8(sizeTag, static_cast<int>(wcslen(sizeTag)));
                }
            }

            const bool fontStateChanged = style.font != previous.font ||
                style.decoration != previous.decoration ||
                style.bold != previous.bold || style.italic != previous.italic;
            if (fontStateChanged) {
                const bool isBaseFontState = style.font == g_baseStyle.font &&
                    style.decoration == g_baseStyle.decoration &&
                    style.bold == g_baseStyle.bold && style.italic == g_baseStyle.italic;
                if (isBaseFontState) {
                    output += "<@>";
                }
                else {
                    const std::wstring decoration = std::to_wstring(style.decoration) +
                        (style.bold ? L"B" : L"") + (style.italic ? L"I" : L"");
                    const std::wstring fontTag = L"<@" + style.font + L"," +
                        decoration + L">";
                    output += WideToUtf8(fontTag.c_str(),
                        static_cast<int>(fontTag.size()));
                }
            }

            if (style.color != previous.color ||
                style.outlineColor != previous.outlineColor) {
                if (style.color == g_baseStyle.color &&
                    style.outlineColor == g_baseStyle.outlineColor) {
                    output += "<#>";
                }
                else {
                    const std::wstring colorTag = L"<#" + ColorToHex(style.color) +
                        L"," + ColorToHex(style.outlineColor) + L">";
                    output += WideToUtf8(colorTag.c_str(),
                        static_cast<int>(colorTag.size()));
                }
            }

            const std::wstring segment = text.substr(start, end - start);
            output += WideToUtf8(segment.c_str(), static_cast<int>(segment.size()));
            previous = style;
            start = end;
        }

        CHARRANGE endRange{ static_cast<LONG>(text.size()),
            static_cast<LONG>(text.size()) };
        SendMessageW(g_editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&endRange));
        return output;
    }
    std::wstring ReadComboText(HWND combo) {
        const int length = GetWindowTextLengthW(combo);
        if (length <= 0) {
            return {};
        }
        std::wstring value(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(combo, value.data(), length + 1);
        value.resize(static_cast<size_t>(length));
        return value;
    }

    void ApplySelectionFormat(const CHARFORMAT2W& format, const wchar_t* success,
        bool refocus = true) {
        CHARRANGE range = {};
        SendMessageW(g_editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        if (range.cpMin == range.cpMax) {
            SetStatus(L"入力欄で装飾する文字列を選択してください。");
            return;
        }
        SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_SELECTION,
            reinterpret_cast<LPARAM>(&format));
        if (refocus) {
            SetFocus(g_editor);
        }
        SetStatus(success);
    }

    void ToggleStyle(DWORD mask, DWORD effect) {
        CHARRANGE range = {};
        SendMessageW(g_editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        if (range.cpMin == range.cpMax) {
            SetStatus(L"入力欄で装飾する文字列を選択してください。");
            return;
        }

        CHARFORMAT2W current = {};
        current.cbSize = sizeof(current);
        SendMessageW(g_editor, EM_GETCHARFORMAT, SCF_SELECTION,
            reinterpret_cast<LPARAM>(&current));

        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = mask;
        const bool enable = !(current.dwMask & mask) || !(current.dwEffects & effect);
        format.dwEffects = enable ? effect : 0;
        SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_SELECTION,
            reinterpret_cast<LPARAM>(&format));
        SetFocus(g_editor);
        SetStatus(enable ? L"選択範囲にスタイルを適用しました。" :
            L"選択範囲からスタイルを解除しました。");
    }

    void ApplyFont(bool refocus = true) {
        std::wstring font = ReadComboText(g_fontCombo);
        if (!font.empty() && font.front() == L'@') {
            font.erase(font.begin());
        }
        if (font.empty()) {
            return;
        }
        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_FACE;
        wcsncpy_s(format.szFaceName, font.c_str(), _TRUNCATE);
        ApplySelectionFormat(format, L"選択範囲のフォントを変更しました。", refocus);
    }

    void ApplySize(bool refocus = true) {
        const std::wstring sizeText = ReadComboText(g_sizeCombo);
        wchar_t* end = nullptr;
        const long size = std::wcstol(sizeText.c_str(), &end, 10);
        if (size < 1 || size > 1000 || end == sizeText.c_str() || *end != L'\0') {
            SetStatus(L"サイズは1から1000の整数で指定してください。");
            return;
        }
        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_SIZE;
        format.yHeight = static_cast<LONG>(size * 20);
        ApplySelectionFormat(format, L"選択範囲のサイズを変更しました。", refocus);
    }

    void ApplyTextColor(COLORREF color) {
        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = color;
        ApplySelectionFormat(format, L"選択範囲の文字色を変更しました。");
    }

    COLORREF ChooseColor(HWND owner, COLORREF initial) {
        CHOOSECOLORW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.rgbResult = initial;
        dialog.lpCustColors = g_customColors;
        dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
        return ChooseColorW(&dialog) ? dialog.rgbResult : initial;
    }

    bool ApplyOutlineColorToSelection(COLORREF color) {
        CHARRANGE range = {};
        SendMessageW(g_editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        if (range.cpMin == range.cpMax) {
            SetStatus(L"入力欄で輪郭色を変更する文字を選択してください。");
            return false;
        }

        SyncOutlineColors();
        const size_t start = range.cpMin < 0 ? 0 : static_cast<size_t>(range.cpMin);
        const size_t end = range.cpMax < 0 ? 0 : static_cast<size_t>(range.cpMax);
        const size_t boundedStart = (std::min)(start, g_outlineColors.size());
        const size_t boundedEnd = (std::min)(end, g_outlineColors.size());
        if (boundedEnd <= boundedStart) {
            SetStatus(L"選択範囲の輪郭色を変更できませんでした。");
            return false;
        }

        for (size_t i = boundedStart; i < boundedEnd; ++i) {
            g_outlineColors[i] = color;
        }
        g_outlineColor = color;
        InvalidateRect(g_editor, nullptr, FALSE);
        SetFocus(g_editor);
        SetStatus(L"選択範囲の輪郭色を変更しました。「反映」で本体に適用します。");
        return true;
    }

    void ChooseAndApplyColor(HWND owner) {
        CHOOSECOLORW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.rgbResult = g_baseStyle.color;
        dialog.lpCustColors = g_customColors;
        dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (ChooseColorW(&dialog)) {
            ApplyTextColor(dialog.rgbResult);
        }
    }

    void ChooseAndApplyOutlineColor(HWND owner) {
        CHARRANGE range = {};
        SendMessageW(g_editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
        if (range.cpMin == range.cpMax) {
            SetStatus(L"入力欄で輪郭色を変更する文字を選択してください。");
            return;
        }

        SyncOutlineColors();
        size_t start = range.cpMin < 0 ? 0 : static_cast<size_t>(range.cpMin);
        size_t end = range.cpMax < 0 ? 0 : static_cast<size_t>(range.cpMax);
        start = (std::min)(start, g_outlineColors.size());
        end = (std::min)(end, g_outlineColors.size());
        if (end <= start) {
            SetStatus(L"選択範囲の輪郭色を変更できませんでした。");
            return;
        }

        CHOOSECOLORW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.rgbResult = g_outlineColors[start];
        dialog.lpCustColors = g_customColors;
        dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (!ChooseColorW(&dialog)) {
            return;
        }
        ApplyOutlineColorToSelection(dialog.rgbResult);
    }

    void ChooseEditorBackground(HWND owner) {
        const COLORREF color = ChooseColor(owner, g_editorBackground);
        g_editorBackground = color;
        SendMessageW(g_editor, EM_SETBKGNDCOLOR, 0, color);
        SetStatus(L"プラグイン入力欄の背景色を変更しました。");
    }

    COLORREF SampleScreenPixel() {
        POINT point = {};
        if (!GetCursorPos(&point)) {
            return CLR_INVALID;
        }
        HDC screen = GetDC(nullptr);
        if (!screen) {
            return CLR_INVALID;
        }
        const COLORREF color = GetPixel(screen, point.x, point.y);
        ReleaseDC(nullptr, screen);
        return color;
    }

    void BeginScreenColorPick(ColorPickTarget target) {
        g_pickTarget = target;
        g_pickingColor = true;
        SetFocus(g_toolbarWindow);
        SetCapture(g_toolbarWindow);
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        const wchar_t* message = L"スポイト: 画面上の色をクリック。Escでキャンセル。";
        if (target == ColorPickTarget::Outline) {
            message = L"輪郭色スポイト: 画面上の色をクリック。Escでキャンセル。";
        }
        else if (target == ColorPickTarget::Background) {
            message = L"背景スポイト: 画面上の色をクリック。Escでキャンセル。";
        }
        SetStatus(message);
    }

    BOOL CALLBACK EnumFontCallback(const LOGFONTW* font,
        const TEXTMETRICW*, DWORD, LPARAM parameter) {
        HWND combo = reinterpret_cast<HWND>(parameter);
        if (font->lfFaceName[0] == L'@') return TRUE;
        if (combo && IsWindow(combo)) {
            const LRESULT found = SendMessageW(combo, CB_FINDSTRINGEXACT,
                static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(font->lfFaceName));
            if (found == CB_ERR) {
                SendMessageW(combo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(font->lfFaceName));
            }
        }
        return TRUE;
    }

    LRESULT CALLBACK ToolbarWindowProc(HWND window, UINT message,
        WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_RICH_EDITOR &&
                HIWORD(wParam) == EN_CHANGE) {
                SyncOutlineColors();
                return 0;
            }
            if (LOWORD(wParam) == kFontCombo &&
                (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_KILLFOCUS)) {
                ApplyFont(false);
                return 0;
            }
            if (LOWORD(wParam) == kSizeCombo &&
                (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_KILLFOCUS)) {
                ApplySize(false);
                return 0;
            }
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                case kBoldButton:
                    ToggleStyle(CFM_BOLD, CFE_BOLD);
                    return 0;
                case kItalicButton:
                    ToggleStyle(CFM_ITALIC, CFE_ITALIC);
                    return 0;
                case kColorButton:
                    ChooseAndApplyColor(window);
                    return 0;
                case kTextEyedropperButton:
                    BeginScreenColorPick(ColorPickTarget::Text);
                    return 0;
                case kBackgroundButton:
                    ChooseEditorBackground(window);
                    return 0;
                case kOutlineColorButton:
                    ChooseAndApplyOutlineColor(window);
                    return 0;
                case kOutlineEyedropperButton:
                    BeginScreenColorPick(ColorPickTarget::Outline);
                    return 0;
                case kOutlinePreviewButton:
                    g_outlinePreviewEnabled = !g_outlinePreviewEnabled;
                    SetWindowTextW(GetDlgItem(window, kOutlinePreviewButton),
                        g_outlinePreviewEnabled ? L"縁表示: ON" : L"縁表示: OFF");
                    if (g_editor) InvalidateRect(g_editor, nullptr, TRUE);
                    SetStatus(g_outlinePreviewEnabled ? L"輪郭プレビューを表示します。" : L"輪郭プレビューを非表示にしました。");
                    return 0;
                case kBackgroundEyedropperButton:
                    BeginScreenColorPick(ColorPickTarget::Background);
                    return 0;
                case kLoadButton:
                    LoadSelectedText();
                    return 0;
                case kApplyButton:
                    ApplyTextToSelectedObject();
                    return 0;
                }
            }
            break;
        case WM_MOUSEMOVE:
            if (g_pickingColor) {
                const COLORREF sample = SampleScreenPixel();
                if (sample != CLR_INVALID) {
                    wchar_t status[96] = {};
                    const wchar_t* format = L"文字色: #%02X%02X%02Xをクリックして適用";
                    if (g_pickTarget == ColorPickTarget::Outline) {
                        format = L"輪郭色: #%02X%02X%02Xをクリックして適用";
                    }
                    else if (g_pickTarget == ColorPickTarget::Background) {
                        format = L"背景色: #%02X%02X%02Xをクリックして適用";
                    }
                    swprintf_s(status, format,
                        GetRValue(sample), GetGValue(sample), GetBValue(sample));
                    SetStatus(status);
                }
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (g_pickingColor) {
                const ColorPickTarget target = g_pickTarget;
                const COLORREF color = SampleScreenPixel();
                g_pickingColor = false;
                ReleaseCapture();
                if (color == CLR_INVALID) {
                    SetStatus(L"画面から色を取得できませんでした。");
                }
                else if (target == ColorPickTarget::Text) {
                    ApplyTextColor(color);
                }
                else if (target == ColorPickTarget::Outline) {
                    ApplyOutlineColorToSelection(color);
                }
                else {
                    g_editorBackground = color;
                    SendMessageW(g_editor, EM_SETBKGNDCOLOR, 0, color);
                    SetStatus(L"プラグイン入力欄の背景色をスポイトで変更しました。");
                }
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (g_pickingColor && wParam == VK_ESCAPE) {
                g_pickingColor = false;
                ReleaseCapture();
                SetStatus(L"スポイトをキャンセルしました。");
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (g_pickingColor) {
                SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            }
            break;
        case WM_CAPTURECHANGED:
            if (g_pickingColor) {
                g_pickingColor = false;
                SetStatus(L"スポイトを終了しました。");
            }
            break;
        case WM_SIZE:
            if (g_editor) {
                const int width = LOWORD(lParam);
                const int height = HIWORD(lParam);
                SetWindowPos(g_editor, nullptr, g_editorLeft, g_editorTop,
                    (std::max)(100, width - g_editorLeft - g_editorRightMargin),
                    (std::max)(40, height - g_editorTop - g_editorBottomMargin),
                    SWP_NOZORDER);
            }
            if (g_statusLabel) {
                const int width = LOWORD(lParam);
                SetWindowPos(g_statusLabel, nullptr, g_statusLeft, g_statusTop,
                    (std::max)(100, width - g_statusLeft - g_statusRightMargin),
                    g_statusHeight, SWP_NOZORDER);
            }
            break;
        case WM_DESTROY:
            g_toolbarWindow = nullptr;
            g_editor = nullptr;
            g_fontCombo = nullptr;
            g_sizeCombo = nullptr;
            g_statusLabel = nullptr;
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void PopulateFontCombo() {
        if (g_fontsLoaded || !g_fontCombo || !IsWindow(g_fontCombo)) return;
        HDC dc = GetDC(g_toolbarWindow);
        if (!dc) return;
        LOGFONTW font = {};
        font.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc, &font,
            reinterpret_cast<FONTENUMPROCW>(EnumFontCallback),
            reinterpret_cast<LPARAM>(g_fontCombo), 0);
        ReleaseDC(g_toolbarWindow, dc);
        g_fontsLoaded = true;
    }

    LRESULT CALLBACK LightBorderSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR referenceData);

    INT_PTR CALLBACK ToolbarDialogProc(HWND window, UINT message,
    WPARAM wParam, LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        g_toolbarWindow = window;
        g_panelBrush = CreateSolidBrush(RGB(38, 38, 38));
        g_buttonBrush = CreateSolidBrush(kButtonColor);
        g_buttonPressedBrush = CreateSolidBrush(kButtonPressedColor);
        g_borderBrush = CreateSolidBrush(kControlBorderColor);
        return TRUE;
    }

    if (message == kPopulateFontsMessage) {
        PopulateFontCombo();
        return TRUE;
    }

    if (message == WM_DRAWITEM) {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!item || item->CtlType != ODT_BUTTON) return FALSE;

        const bool pressed = (item->itemState & ODS_SELECTED) != 0;
        HBRUSH brush = pressed ? g_buttonPressedBrush : g_buttonBrush;
        FillRect(item->hDC, &item->rcItem, brush);
        RECT frame = item->rcItem;
        FrameRect(item->hDC, &frame, g_borderBrush);

        if (item->CtlID == kTextEyedropperButton ||
            item->CtlID == kBackgroundEyedropperButton ||
            item->CtlID == kOutlineEyedropperButton) {
            if (g_eyedropperIcon) {
                const int iconSize = 16;
                const int x = (item->rcItem.left + item->rcItem.right - iconSize) / 2 + (pressed ? 1 : 0);
                const int y = (item->rcItem.top + item->rcItem.bottom - iconSize) / 2 + (pressed ? 1 : 0);
                DrawIconEx(item->hDC, x, y, g_eyedropperIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }
        } else {
            wchar_t caption[128] = {};
            GetWindowTextW(item->hwndItem, caption, static_cast<int>(_countof(caption)));
            RECT textRect = item->rcItem;
            InflateRect(&textRect, -3, -1);
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, kButtonTextColor);
            DrawTextW(item->hDC, caption, -1, &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        if (item->itemState & ODS_FOCUS) {
            RECT focusRect = item->rcItem;
            InflateRect(&focusRect, -4, -4);
            DrawFocusRect(item->hDC, &focusRect);
        }
        return TRUE;
    }
    if (message == WM_CTLCOLORLISTBOX) {
        HWND listBox = reinterpret_cast<HWND>(lParam);
        if (listBox) SetWindowSubclass(listBox, LightBorderSubclassProc, 1, 0);
    }
    if (message == WM_CTLCOLORDLG || message == WM_CTLCOLORSTATIC ||
        message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX) {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(240, 240, 240));
        SetBkColor(dc, RGB(38, 38, 38));
        return reinterpret_cast<INT_PTR>(g_panelBrush);
    }

    ToolbarWindowProc(window, message, wParam, lParam);
    switch (message) {
    case WM_COMMAND:
    case WM_SIZE:
    case WM_CAPTURECHANGED:
        return TRUE;
    case WM_DESTROY:
        if (g_panelBrush) {
            DeleteObject(g_panelBrush);
            g_panelBrush = nullptr;
        }
        if (g_buttonBrush) {
            DeleteObject(g_buttonBrush);
            g_buttonBrush = nullptr;
        }
        if (g_buttonPressedBrush) {
            DeleteObject(g_buttonPressedBrush);
            g_buttonPressedBrush = nullptr;
        }
        if (g_borderBrush) {
            DeleteObject(g_borderBrush);
            g_borderBrush = nullptr;
        }
        return TRUE;
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
    case WM_KEYDOWN:
    case WM_SETCURSOR:
        return g_pickingColor ? TRUE : FALSE;
    default:
        return FALSE;
    }
}
LRESULT CALLBACK LightBorderSubclassProc(HWND window, UINT message,
    WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR) {
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_PAINT && window == g_editor && g_gdiplusToken &&
        !g_drawingOutlinePreview) {
        DrawOutlinePreview(window);
    }
    if ((message == WM_NCPAINT || message == WM_PAINT) && g_borderBrush) {
        // Draw over themed control borders, which may be painted in the client area.
        HDC dc = message == WM_NCPAINT ? GetWindowDC(window) : GetDC(window);
        if (dc) {
            RECT borderRect = {};
            if (message == WM_NCPAINT) {
                RECT windowRect = {};
                if (GetWindowRect(window, &windowRect)) {
                    borderRect = { 0, 0, windowRect.right - windowRect.left,
                        windowRect.bottom - windowRect.top };
                }
            } else {
                GetClientRect(window, &borderRect);
            }
            FrameRect(dc, &borderRect, g_borderBrush);
            ReleaseDC(window, dc);
        }
    }    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, LightBorderSubclassProc, subclassId);
    }
    return result;
}
void RegisterToolbar(HOST_APP_TABLE* host) {
        if (!host || !host->register_window_client || !host->create_edit_handle) {
            return;
        }

        g_richEditModule = LoadLibraryW(L"Msftedit.dll");
        if (!g_richEditModule) {
            return;
        }

        if (!g_gdiplusToken) {
            Gdiplus::GdiplusStartupInput startupInput;
            Gdiplus::GdiplusStartup(&g_gdiplusToken, &startupInput, nullptr);
        }

        g_toolbarWindow = CreateDialogParamW(g_module,
            MAKEINTRESOURCEW(IDD_RICHTEXT_TOOLBAR), nullptr,
            ToolbarDialogProc, 0);
        if (!g_toolbarWindow) {
            return;
        }

        g_eyedropperIcon = static_cast<HICON>(LoadImageW(g_module, MAKEINTRESOURCEW(IDI_RICHTEXT_EYEDROPPER), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        if (g_eyedropperIcon) {
        }
        g_fontCombo = GetDlgItem(g_toolbarWindow, kFontCombo);
        g_sizeCombo = GetDlgItem(g_toolbarWindow, kSizeCombo);
        g_statusLabel = GetDlgItem(g_toolbarWindow, IDC_RICH_STATUS);
        g_editor = GetDlgItem(g_toolbarWindow, IDC_RICH_EDITOR);
        for (HWND control : { g_fontCombo, g_sizeCombo, g_editor }) {
            if (control) {
                SetWindowSubclass(control, LightBorderSubclassProc, 1, 0);
                RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
            }
        }
        RECT clientRect = {};
        RECT childRect = {};
        GetClientRect(g_toolbarWindow, &clientRect);
        if (g_editor && GetWindowRect(g_editor, &childRect)) {
            MapWindowPoints(nullptr, g_toolbarWindow, reinterpret_cast<POINT*>(&childRect), 2);
            g_editorLeft = childRect.left;
            g_editorTop = childRect.top;
            g_editorRightMargin = clientRect.right - childRect.right;
            g_editorBottomMargin = clientRect.bottom - childRect.bottom;
        }
        if (g_statusLabel && GetWindowRect(g_statusLabel, &childRect)) {
            MapWindowPoints(nullptr, g_toolbarWindow, reinterpret_cast<POINT*>(&childRect), 2);
            g_statusLeft = childRect.left;
            g_statusTop = childRect.top;
            g_statusRightMargin = clientRect.right - childRect.right;
            g_statusHeight = childRect.bottom - childRect.top;
        }
        for (const wchar_t* size : { L"12", L"16", L"20", L"24", L"32", L"40", L"48", L"64", L"72" }) {
            SendMessageW(g_sizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(size));
        }
        SetWindowTextW(g_sizeCombo, L"32");
        if (g_editor) {
            SendMessageW(g_editor, EM_EXLIMITTEXT, 0, 1024 * 1024);
            SendMessageW(g_editor, EM_SETBKGNDCOLOR, 0, g_editorBackground);
            CHARFORMAT2W base = {};
            base.cbSize = sizeof(base);
            base.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
            base.yHeight = g_baseStyle.size * 20;
            base.crTextColor = g_baseStyle.color;
            wcsncpy_s(base.szFaceName, g_baseStyle.font.c_str(), _TRUNCATE);
            SendMessageW(g_editor, EM_SETCHARFORMAT, SCF_ALL,
                reinterpret_cast<LPARAM>(&base));
        }

        if (g_fontCombo) {
            std::wstring fontName = g_baseStyle.font;
            if (!fontName.empty() && fontName.front() == L'@') {
                fontName.erase(fontName.begin());
            }
            SetWindowTextW(g_fontCombo, fontName.c_str());
        }

        host->register_window_client(kWindowName, g_toolbarWindow);
        g_editHandle = host->create_edit_handle();
        // Defer potentially slow system font enumeration until plugin registration returns.
        PostMessageW(g_toolbarWindow, kPopulateFontsMessage, 0, 0);
    }

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"Office-Style RichText",
    L"Office-Style RichText version 1.1.0"
};

EXTERN_C __declspec(dllexport) DWORD RequiredVersion() {
    return 2003300;
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) {
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    if (g_eyedropperIcon) {
        DestroyIcon(g_eyedropperIcon);
        g_eyedropperIcon = nullptr;
    }
    if (g_richEditModule) {
        FreeLibrary(g_richEditModule);
        g_richEditModule = nullptr;
    }
}

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    RegisterToolbar(host);
}
