#include "resource.h"

#include <QDesktopWidget>
#include <windows.h>

// external Qt routine
QPixmap qt_pixmapFromWinHICON(HICON icon);

// static members
QPixmap* Resources::pxOpenfile      = NULL;
QPixmap* Resources::pxMpegtsFile    = NULL;
short    Resources::desktopX        = 1280;
short    Resources::desktopY        = 1024;

// call once after in the main window ctor
void Resources::Init()
{
    QDesktopWidget desk;
    QRect rcScreen( desk.screenGeometry() );
    desktopX = rcScreen.height()+1;
    desktopY = rcScreen.width()+1;

    HMODULE hInst = ::GetModuleHandleW(NULL);
    HICON hIco = (HICON)::LoadImageW(hInst, MAKEINTRESOURCE(IDI_OPENFILE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
    pxOpenfile = new QPixmap( qt_pixmapFromWinHICON(hIco) );

    hIco = (HICON)::LoadImageW(hInst, MAKEINTRESOURCE(IDI_MPEGTSFILE), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
    pxMpegtsFile = new QPixmap( qt_pixmapFromWinHICON(hIco) );
}
