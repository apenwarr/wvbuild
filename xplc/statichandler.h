/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * XPLC - Cross-Platform Lightweight Components
 * Copyright (C) 2000-2003, Pierre Phaneuf
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __XPLC_STATICHANDLER_H__
#define __XPLC_STATICHANDLER_H__

#include <xplc/IStaticServiceHandler.h>
#include "objectnode.h"

class StaticServiceHandler: public IStaticServiceHandler {
  IMPLEMENT_IOBJECT(StaticServiceHandler);
private:
  ObjectNode* objects;
public:
  StaticServiceHandler():
    objects(0) {
  }
  virtual ~StaticServiceHandler();
  /* IServiceHandler */
  virtual IObject* getObject(const UUID&);
  /* IStaticServiceHandler */
  virtual void addObject(const UUID&, IObject*);
  virtual void removeObject(const UUID&);
};

#endif /* __XPLC_STATICHANDLER_H__ */
