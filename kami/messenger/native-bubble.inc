// Inserted into llchathistory.cpp by apply-imessage-style.py.
// Native controls only: no web view, timers, shaders or per-frame animation.
#include "llui.h"

class LLKamiMessageBubble final : public LLPanel
{
public:
    LLKamiMessageBubble(const LLChat& chat, const LLStyle::Params& style, bool outgoing, const LLSD& args)
        : LLPanel(), mOutgoing(outgoing)
    {
        setName("kami_message_bubble");
        setMouseOpaque(false);
        LLPanel::Params panel;
        panel.name = "bubble_surface";
        panel.background_visible = true;
        panel.background_opaque = true;
        const LLUIColor color(mOutgoing ? LLColor4(0.04f, 0.34f, 0.78f, 1.f) :
                                           LLColor4(0.16f, 0.18f, 0.23f, 1.f));
        panel.bg_opaque_color = color;
        panel.bg_opaque_image_overlay = color;
        panel.bg_opaque_image = LLUI::getUIImage("Rounded_Square").get();
        mSurface = LLUICtrlFactory::create<LLPanel>(panel, this);

        LLTextEditor::Params body;
        body.name = "bubble_text";
        body.read_only = true;
        body.allow_scroll = false;
        body.bg_visible = false;
        body.border_visible = false;
        body.wrap = true;
        body.parse_urls = true;
        body.parse_highlights = true;
        body.use_emoji = true;
        body.show_context_menu = true;
        body.h_pad = 0;
        body.v_pad = 0;
        body.max_text_length = 65536;
        body.font = LLViewerChat::getChatFont();
        body.text_readonly_color = LLUIColor(LLColor4::white);
        mBody = LLUICtrlFactory::create<LLTextEditor>(body, mSurface);
        LLStyle::Params text_style(style);
        const LLUIColor text_color(chat.mChatStyle == CHAT_STYLE_HISTORY ?
            LLColor4(0.78f, 0.80f, 0.84f, 1.f) : LLColor4::white);
        text_style.color(text_color);
        text_style.readonly_color(text_color);
        mBody->setText(chat.mText, text_style);
        mNaturalWidth = LLViewerChat::getChatFont()->getWidth(chat.mText) + 28;

        LLTextBox::Params meta;
        meta.name = "bubble_timestamp";
        meta.font = LLFontGL::getFontSansSerifSmall();
        meta.text_color = LLUIColor(LLColor4(0.67f, 0.70f, 0.76f, 1.f));
        meta.mouse_opaque = false;
        meta.parse_urls = false;
        meta.use_ellipses = true;
        mMeta = LLUICtrlFactory::create<LLTextBox>(meta, this);
        std::string label;
        if (args["show_names_for_p2p_conv"].asBoolean())
            label = mOutgoing ? "You" : chat.mFromName;
        if (args["show_time"].asBoolean())
            label += (label.empty() ? "" : "  ") + chat.mTimeStr;
        mMeta->setText(label);
        mMeta->setHAlign(mOutgoing ? LLFontGL::RIGHT : LLFontGL::LEFT);
    }

    // Called on append and transcript resize, not in draw(). The editor's
    // actual wrapped height handles multiline text, URLs and emoji.
    void fit(S32 available)
    {
        if (mAvailable == available) return;
        mAvailable = available;
        const S32 bubble_width = llclamp(mNaturalWidth, llmin(100, available),
                                        llmax(llmin(100, available), available * 4 / 5));
        mBody->reshape(llmax(20, bubble_width - 24), 20, false);
        const S32 text_height = llmax(20, mBody->getTextBoundingRect().getHeight());
        const S32 bubble_height = text_height + 20;
        LLPanel::reshape(available, bubble_height + 20, false);
        mSurface->reshape(bubble_width, bubble_height, false);
        mSurface->setOrigin(mOutgoing ? available - bubble_width : 0, 20);
        mBody->reshape(llmax(20, bubble_width - 24), text_height, false);
        mBody->setOrigin(12, 10);
        mMeta->reshape(bubble_width, 16, false);
        mMeta->setOrigin(mOutgoing ? available - bubble_width : 0, 0);
    }

private:
    bool mOutgoing;
    S32 mAvailable = -1;
    S32 mNaturalWidth = 100;
    LLPanel* mSurface = nullptr;
    LLTextEditor* mBody = nullptr;
    LLTextBox* mMeta = nullptr;
};
