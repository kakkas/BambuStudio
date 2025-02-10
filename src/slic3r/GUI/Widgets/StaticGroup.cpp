#include "StaticGroup.hpp"

StaticGroup::StaticGroup(wxWindow *parent, wxWindowID id, const wxString &label)
    : wxStaticBox(parent, id, label)
{
    SetBackgroundColour(*wxWHITE);
    SetForegroundColour("#CECECE");
#ifdef __WXMSW__
    Bind(wxEVT_PAINT, &StaticGroup::OnPaint, this);
#endif
}

void StaticGroup::ShowBadge(bool show)
{
#ifdef __WXMSW__
    if (show)
        badge = ScalableBitmap(this, "badge", 18);
    else
        badge = ScalableBitmap{};
    Refresh();
#endif
#ifdef __WXOSX__
    if (show && badge == nullptr) {
        badge = new ScalableButton(this, wxID_ANY, "badge", wxEmptyString, wxDefaultSize, wxDefaultPosition, wxBU_EXACTFIT | wxNO_BORDER, false, 18);
        badge->SetSize(badge->GetBestSize());
        badge->SetBackgroundColour("#F7F7F7");
        LayoutBadge();
    }
    if (badge)
        badge->Show(show);
#endif
}

#ifdef __WXMSW__

void StaticGroup::OnPaint(wxPaintEvent &evt)
{
    wxStaticBox::OnPaint(evt);
    if (badge.bmp().IsOk()) {
        auto s = badge.bmp().GetScaledSize();
        wxPaintDC dc(this);
        dc.DrawBitmap(badge.bmp(), GetSize().x - s.x, 8);
    }
}

void StaticGroup::PaintForeground(wxDC &dc, const struct tagRECT &rc)
{
    wxStaticBox::PaintForeground(dc, rc);
    auto mdc = dynamic_cast<wxMemoryDC *>(&dc);
    auto image = mdc->GetSelectedBitmap().ConvertToImage();
    // Found border coords
    int top = 0;
    int left = 0;
    int right = rc.right - 1;
    int bottom = rc.bottom - 1;
    auto blue  = GetBackgroundColour().Blue();
    while (image.GetBlue(0, top) == blue && top < bottom) ++top;
    while (image.GetBlue(left, top) != blue && left < right) ++left; // --left; // fix start
    while (image.GetBlue(right, top) != blue && right > 0) --right;
    ++right;
    while (image.GetBlue(0, bottom) == blue && bottom > 0) --bottom;
    // Draw border with foreground color
    wxPoint polygon[] = { {left, top}, {0, top}, {0, bottom}, {rc.right - 1, bottom}, {rc.right - 1, top}, {right, top} };
    dc.SetPen(wxPen(GetForegroundColour(), 1));
    for (int i = 1; i < 6; ++i) {
        if (i == 4) // fix bottom right corner
            ++polygon[i - 1].y;
        dc.DrawLine(polygon[i - 1], polygon[i]);
    }
}

#endif

#ifdef __WXOSX__

void StaticGroup::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxStaticBox::DoSetSize(x, y, width, height, sizeFlags);
    if (badge)
        LayoutBadge();
}

void StaticGroup::LayoutBadge()
{
    auto size  = GetSize();
    auto size2 = size - badge->GetSize();
    badge->SetPosition({size2.x - 6, 0});
}

#endif
