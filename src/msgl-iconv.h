/* Message list character set conversion.
   Copyright (C) 2001 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _MSGL_ICONV_H
#define _MSGL_ICONV_H

#include "message.h"

extern void
       iconv_message_list PARAMS ((message_list_ty *mlp,
				   const char *canon_to_code));
extern msgdomain_list_ty *
       iconv_msgdomain_list PARAMS ((msgdomain_list_ty *mdlp,
				     const char *to_code));

#endif /* _MSGL_ICONV_H */
