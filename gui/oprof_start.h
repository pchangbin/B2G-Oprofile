/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef OPROF_START_H
#define OPROF_START_H

#include <vector>

#include "ui/oprof_start.base.h"
#include "oprof_start_config.h"
#include "persistent_config.h"
#include "../op_user.h"

class QIntValidator;
class QListViewItem;

/// a struct describing a particular event type
struct op_event_descr {
	op_event_descr();

	/// bit mask of allowed counters 
	uint counter_mask;
	/// hardware event number 
	u8 val;
	/// unit mask values if applicable 
	const op_unit_mask * unit;
	/// unit mask descriptions if applicable
	const op_unit_desc * um_descr;
	/// name of event 
	std::string name;
	/// description of event
	std::string help_str;
	/// minimum counter value
	uint min_count;
};

class oprof_start : public oprof_start_base
{
	Q_OBJECT

public:
	oprof_start();

protected slots:
	/// after selecting to choose a file/dir
	void on_choose_file_or_dir();
	/// flush profiler 
	void on_flush_profiler_data();
	/// start profiler 
	void on_start_profiler();
	/// stop profiler
	void on_stop_profiler();
	/// the counter combo has been activated
	void counter_selected(int);
	/// an event has been selected 
	void event_selected(QListViewItem *); 
	/// the mouse is over an event 
	void event_over(QListViewItem *); 
	/// enabled has been changed
	void enabled_toggled(bool); 

	/// close the dialog
	void accept();

private:
	/// find an event description by name
	const op_event_descr & locate_event(std::string const & name);

	/// update config on user change
	void record_selected_event_config();
	/// update config and validate 
	bool record_config();

	/// calculate unit mask for given event and unit mask part
	void get_unit_mask_part(op_event_descr const & descr, uint num, bool selected, uint & mask);
	/// calculate unit mask for given event
	uint get_unit_mask(op_event_descr const & descr);
	/// set the unit mask widgets for given event
	void setup_unit_masks(op_event_descr const & descr);

	/// update the counter combo at given position
	void set_counter_combo(uint);
 
	/// load the event config file
	void load_event_config_file();
	/// save the event config file 
	bool save_event_config_file();
	/// load the extra config file
	void load_config_file();
	/// save the extra config file
	bool save_config_file();

	//@ validators
	QIntValidator* validate_buffer_size;
	QIntValidator* validate_hash_table_size;
	QIntValidator* validate_event_count;
	QIntValidator* validate_pid_filter;
	QIntValidator* validate_pgrp_filter;
	//@

	/// all available events for this hardware
	std::vector<op_event_descr> v_events;

	/// the current counter in the GUI
	uint current_ctr;
	/// current event selections for each counter
	struct op_event_descr const * current_event[OP_MAX_COUNTERS];
	/// current event configs for each counter
	persistent_config_t<event_setting> event_cfgs[OP_MAX_COUNTERS];
	/// enabled status for each counter
	bool ctr_enabled[OP_MAX_COUNTERS]; 
 
	/// current config
	config_setting config;

	/// the expansion of "~" directory
	std::string user_dir;

	/// CPU type
	int cpu_type;

	/// total number of available HW counters
	uint op_nr_counters;
};

#endif // OPROF_START_H
