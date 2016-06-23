/* -*- c++ -*- */
/*
 * Copyright 2011 Free Software Foundation, Inc.
 *
 * This file is part of gr-eventstream
 *
 * gr-eventstream is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * gr-eventstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gr-eventstream; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <es/es.h>
#include <gnuradio/io_signature.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Create a new instance of es_distributor and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
es_distributor_sptr
es_make_distributor (gr_vector_int iosig, int num_out_ports, bool separate_registration)
{
  return es_distributor_sptr (
    new es_distributor (iosig, num_out_ports, separate_registration));
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr::block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * as few as 0 input/output or as many as 4 input/output. The
 * number of input and output streams are equal.
 */
static const int MIN_IN = 0;	// mininum number of input streams
static const int MAX_IN = 4;	// maximum number of input streams
static const int MIN_OUT = 0;	// minimum number of output streams
static const int MAX_OUT = 4;	// maximum number of output streams

es_distributor::es_distributor (
  gr_vector_int iosig, int num_out_ports, bool separate_registration)
  :
    d_time(0),
    d_num_out_ports(num_out_ports),
    d_num_events_distributed(0),
    d_num_events_registered(0),
    d_separate_registration(separate_registration),
    gr::sync_block ("es_distributor",
      es_make_io_signature(iosig.size(), iosig),
      es_make_io_signature(iosig.size(), iosig))
{
  /* INPUT */
  /* Create an input message port for consuming events. */
  message_port_register_in(pmt::mp("dist_random"));
  set_msg_handler(
    pmt::mp("dist_random"), boost::bind(&es_distributor::dist_msg_random, this, _1));

  /* This block can be set up to have a separate message port for handling
   * event registrations. This should increase performance of this block by
   * decreasing the amount of work that has to be performed to parse the
   * message to determine if the message is an event or an event registration
   * message.
   */
  if (separate_registration)
  {
    message_port_register_in(pmt::mp("dist_all"));
    set_msg_handler(
      pmt::mp("dist_all"), boost::bind(&es_distributor::dist_msg_all, this, _1));
  }

  /* OUTPUT */
  std::string base_out_port_name = "dist_out";
  for (int i = 0; i < d_num_out_ports; i++)
  {
    std::string out_port_name = base_out_port_name;
    if (d_num_out_ports != 1)
    {
      out_port_name = base_out_port_name + int_to_string(i);
    }
    message_port_register_out(pmt::mp(out_port_name.c_str()));
    d_out_ports.push_back(out_port_name);
  }
}

es_distributor::~es_distributor ()
{
}

std::string
es_distributor::int_to_string(int value)
{
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

void
es_distributor::dist_msg_random(pmt_t msg)
{
  if (!d_separate_registration)
  {
    /* Check if registration event. */
    if (
      pmt::is_pair(msg) &&
      (pmt::eqv(pmt::car(msg), pmt::mp("ES_REGISTER_HANDLER"))))
    {
      /* Registration events are sent to all output ports. */
      dist_msg_all(msg);
      return; // We are done.
    }
  }

  /* If we get here then we are assuming the message is to be passed to
   * a single random output port.
   */
  int random_sink = rand() % d_num_out_ports;

  message_port_pub(pmt::mp(d_out_ports[random_sink].c_str()), msg);
  d_num_events_distributed++;
}

void
es_distributor::dist_msg_all(pmt_t msg)
{
  /* If this handler is called, then it is assumed that msg is an event
   * registration message. If it isn't then the receiving block may
   * complain but there is no checking performed in this block.
   */
  uint64_t sz = d_out_ports.size();

  for (int i = 0; i < sz; i++)
  {
    message_port_pub(pmt::mp(d_out_ports[i].c_str()), msg);
  }
  d_num_events_registered++;
}

int
es_distributor::work (
  int noutput_items,
  gr_vector_const_void_star &input_items,
  gr_vector_void_star &output_items)
{
  /* Pass all input samples to the output. */
  size_t n_chains = output_items.size();

  for (int i = 0; i < n_chains; i++)
  {
    int item_size = input_signature()->sizeof_stream_item(i);
    void *ii = (void *) input_items[i];
    void *oi = (void *) output_items[i];

    memcpy(oi, ii, noutput_items * item_size);
  }

  d_time += noutput_items;
  return noutput_items;
}

uint64_t
es_distributor::nevents_registered()
{
  return d_num_events_registered;
}

uint64_t
es_distributor::nevents_distributed()
{
  return d_num_events_distributed;
}