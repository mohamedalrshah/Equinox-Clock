
/*
Copyright (c) 2010 Donatien Garnier (donatiengar [at] gmail [dot] com)
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
 
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
@file
ZG2100 LwIP Driver
*/
//Donatien Garnier 2010

#ifndef ZG_NET_H
#define ZG_NET_H

#include "zg_defs.h"

typedef signed char err_t;

//To be called by Lwip
///Transfers an ethernet frame: converts into ZG frame & TXs it
static err_t zg_output(struct netif *netif, struct pbuf *p); 

//Callback from zg_drv
///On reception of a ZG frame: converts into Eth frame & feeds lwip
void zg_on_input(byte* buf, int len);

///Initializes network interface
err_t zg_net_init(struct netif *netif);

#endif
