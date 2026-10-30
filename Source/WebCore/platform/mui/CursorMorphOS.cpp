/*
 * Copyright (C) 2009-2013 Fabien Coeurjoly.
 * Copyright (C) 2008 Pleyo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "Cursor.h"
#include <wtf/Assertions.h>
#include "Logging.h"

#include <cstdio>
#include <string.h>
#include <proto/intuition.h>
#include <intuition/pointerclass.h>
#if OS(AMIGAOS3)
#include "Image.h"
#include <libraries/mui.h>
#endif

namespace WebCore {

void Cursor::ensurePlatformCursor() const
{
#if OS(AMIGAOS3)
    // MUI owns the native pointer objects. MorphOS's numeric Intuition
    // pointer types and OS4's pointer objects are different interfaces.
    ULONG pointerType;
    switch (m_type) {
    case Cross: pointerType = MUIV_PointerType_Cross; break;
    case Hand: pointerType = MUIV_PointerType_Link; break;
    case IBeam: pointerType = MUIV_PointerType_Text; break;
    case VerticalText: pointerType = MUIV_PointerType_VerticalText; break;
    case Wait: pointerType = MUIV_PointerType_Busy; break;
    case Progress: pointerType = MUIV_PointerType_Progress; break;
    case Help: pointerType = MUIV_PointerType_Help; break;
    case EastResize: case EastPanning: pointerType = MUIV_PointerType_EastResize; break;
    case WestResize: case WestPanning: pointerType = MUIV_PointerType_WestResize; break;
    case NorthResize: case NorthPanning: pointerType = MUIV_PointerType_NorthResize; break;
    case SouthResize: case SouthPanning: pointerType = MUIV_PointerType_SouthResize; break;
    case NorthEastResize: case NorthEastPanning: pointerType = MUIV_PointerType_NorthEastResize; break;
    case NorthWestResize: case NorthWestPanning: pointerType = MUIV_PointerType_NorthWestResize; break;
    case SouthEastResize: case SouthEastPanning: pointerType = MUIV_PointerType_SouthEastResize; break;
    case SouthWestResize: case SouthWestPanning: pointerType = MUIV_PointerType_SouthWestResize; break;
    case NorthSouthResize: pointerType = MUIV_PointerType_NorthSouthResize; break;
    case EastWestResize: pointerType = MUIV_PointerType_EastWestResize; break;
    case NorthEastSouthWestResize: pointerType = MUIV_PointerType_NorthEastSouthWestResize; break;
    case NorthWestSouthEastResize: pointerType = MUIV_PointerType_NorthWestSouthEastResize; break;
    case ColumnResize: pointerType = MUIV_PointerType_ColumnResize; break;
    case RowResize: pointerType = MUIV_PointerType_RowResize; break;
    case Move: case MiddlePanning: pointerType = MUIV_PointerType_ScrollAll; break;
    case Cell: pointerType = MUIV_PointerType_Cell; break;
    case ContextMenu: pointerType = MUIV_PointerType_ContextMenu; break;
    case Alias: pointerType = MUIV_PointerType_Alias; break;
    case NoDrop: pointerType = MUIV_PointerType_NoDrop; break;
    case NotAllowed: pointerType = MUIV_PointerType_NotAllowed; break;
    case Copy: pointerType = MUIV_PointerType_Copy; break;
    case None: pointerType = MUIV_PointerType_None; break;
    case ZoomIn: pointerType = MUIV_PointerType_ZoomIn; break;
    case ZoomOut: pointerType = MUIV_PointerType_ZoomOut; break;
    case Grab: pointerType = MUIV_PointerType_Hand; break;
    case Grabbing: pointerType = MUIV_PointerType_DragAndDrop; break;
    default: pointerType = MUIV_PointerType_Normal; break;
    }
    m_platformCursor = reinterpret_cast<PlatformCursor>(pointerType);
#elif !OS(AROS) && !OS(AMIGAOS)
    ULONG pointertype;

    switch(m_type)
    {
        default:
        case Pointer:
            pointertype = POINTERTYPE_NORMAL;
            break;
        case Hand:
            pointertype = POINTERTYPE_SELECTLINK;
            break;
        case Cross:
            pointertype = POINTERTYPE_AIMING;
            break;
        case Wait:
            pointertype = POINTERTYPE_BUSY;
            break;
        case Help:
            pointertype = POINTERTYPE_HELP;
            break;
        case IBeam:
        case VerticalText:
            pointertype = POINTERTYPE_SELECTTEXT;
            break;
        case Move:
        case Grab:
        case Grabbing:
            pointertype = POINTERTYPE_MOVE;
            break;
        case EastResize:
        case WestResize:
        case EastWestResize:
        case ColumnResize:
            pointertype = POINTERTYPE_HORIZONTALRESIZE;
            break;
        case NorthResize:
        case SouthResize:
        case NorthSouthResize:
        case RowResize:
            pointertype = POINTERTYPE_VERTICALRESIZE;
            break;
        case NorthEastResize:
        case SouthWestResize:
        case NorthEastSouthWestResize:
            pointertype = POINTERTYPE_DIAGONALRESIZE1;
            break;
        case SouthEastResize:
        case NorthWestResize:
        case NorthWestSouthEastResize:
            pointertype = POINTERTYPE_DIAGONALRESIZE2;
            break;
        case Progress:
            pointertype = POINTERTYPE_WORKING;
            break;
        case NoDrop:
        case NotAllowed:
            pointertype = POINTERTYPE_NOTAVAILABLE;
            break;
    }

    m_platformCursor = (void *) pointertype;
#endif
}

#if OS(AMIGAOS3)
Cursor& Cursor::operator=(const Cursor&) = default;
#else
Cursor& Cursor::operator=(const Cursor& other)
{
    //fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
    m_platformCursor = other.m_platformCursor;
    return *this;
}
#endif

}
