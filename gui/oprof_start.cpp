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

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

#include <sstream>
#include <iostream>
#include <fstream>

#include <qcombobox.h> 
#include <qlistbox.h>
#include <qfiledialog.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qtabwidget.h>
#include <qmessagebox.h>
#include <qvalidator.h>
#include <qlabel.h>

#include "oprof_start.h"

// TODO: some ~0u here for CRT_ALL
// jbl: what does this mean ?

namespace {

inline double ratio(double x1, double x2)
{
	return fabs(((x1 - x2) / x2)) * 100;
}

bool check_and_create_config_dir()
{
	// create the directory if necessary.
	std::string dir = get_user_filename(".oprofile");

	if (access(dir.c_str(), F_OK)) {
		if (mkdir(dir.c_str(), 0700)) {
			std::ostringstream out;
			out << "unable to create " << dir << " directory: ";
			QMessageBox::warning(0, 0, out.str().c_str());

			return false;
		}
	}

	return true;
}

std::string const format(std::string const & orig, uint const maxlen)
{
	string text(orig);

	std::istringstream ss(text);
	std::vector<std::string> lines;

	std::string oline;
	std::string line;

	while (getline(ss, oline)) {
		if (line.size() + oline.size() < maxlen) {
			lines.push_back(line + oline);
			line.erase();
		} else {
			lines.push_back(line);
			line.erase();
			std::string s;
			std::string word;
			std::istringstream oss(oline);
			while (oss >> word) {
				if (line.size() + word.size() > maxlen) {
					lines.push_back(line);
					line.erase();
				}
				line += word + " ";
			}
		}
	}

	if (line.size())
		lines.push_back(line);

	std::string ret;

	for(std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
		ret += *it + "\n";

	return ret;
}


int do_exec_command(const std::string& cmd)
{
	std::ostringstream out;
	std::ostringstream err;

	int ret = exec_command(cmd, out, err);

	// FIXME: err is empty e.g. if you are not root !!
 
	if (ret) {
		std::string error = "Failed: with error \"" + err.str() + "\"\n";
		error += "Command was :\n\n" + cmd + "\n";

		QMessageBox::warning(0, 0, format(error, 50).c_str());
	}

	return ret;
}

QString do_open_file_or_dir(QString base_dir, bool dir_only)
{
	QString result;

	if (dir_only) {
		result = QFileDialog::getExistingDirectory(base_dir, 0,
			   "open_file_or_dir", "Get directory name", true);
	} else {
		result = QFileDialog::getOpenFileName(base_dir, 0, 0,
			   "open_file_or_dir", "Get filename");
	}

	return result;
}

// like posix shell utils basename, do not append trailing '/' to result.
std::string basename(const std::string& path_name)
{
	std::string result = path_name;

	// remove all trailing '/'
	size_t last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos) {
		while (last_delimiter && result[last_delimiter] == '/')
			--last_delimiter;

		result.erase(last_delimiter);
	}

	last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos)
		result.erase(last_delimiter);

	return result;
}

} // anonymous namespace

op_event_descr::op_event_descr()
	: 
	counter_mask(0),
	val(0),
	unit(0),
	um_descr(0),
	min_count(0)
{
}

oprof_start::oprof_start()
	:
	oprof_start_base(0, 0, false, 0),
	validate_buffer_size(new QIntValidator(buffer_size_edit)),
	validate_hash_table_size(new QIntValidator(hash_table_size_edit)),
	validate_event_count(new QIntValidator(event_count_edit)),
	validate_pid_filter(new QIntValidator(pid_filter_edit)),
	validate_pgrp_filter(new QIntValidator(pgrp_filter_edit)),
	current_ctr(0),
	cpu_type(op_get_cpu_type()),
	op_nr_counters(2)
{
	for (uint i = 0; i < OP_MAX_COUNTERS; ++i) {
		current_event[i] = 0;
		ctr_enabled[i] = 0;
	}
 
	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	// validator range/value are set only when we have build the
	// description of events.
	buffer_size_edit->setValidator(validate_buffer_size);
	hash_table_size_edit->setValidator(validate_hash_table_size);
	event_count_edit->setValidator(validate_event_count);
	pid_filter_edit->setValidator(validate_pid_filter);
	pgrp_filter_edit->setValidator(validate_pgrp_filter);

	int cpu_mask = 1 << cpu_type;

	load_config_file();

	// setup the configuration page.
	kernel_filename_edit->setText(config.kernel_filename.c_str());
	map_filename_edit->setText(config.map_filename.c_str());

	// this do not work perhaps we need to derivate QIntValidator to get a
	// better handling if the validation?
//	validate_buffer_size->setValue(config.buffer_size);

	buffer_size_edit->setText(QString().setNum(config.buffer_size));
	hash_table_size_edit->setText(QString().setNum(config.hash_table_size));
	pid_filter_edit->setText(QString().setNum(config.pid_filter));
	pgrp_filter_edit->setText(QString().setNum(config.pgrp_filter));
	base_opd_dir_edit->setText(config.base_opd_dir.c_str());
	ignore_daemon_samples_cb->setChecked(config.ignore_daemon_samples);
	kernel_only_cb->setChecked(config.kernel_only);

	// the unit mask check boxes
	check0->hide();
	check1->hide();
	check2->hide();
	check3->hide();
	check4->hide();
	check5->hide();
	check6->hide(); 
 
	// build from stuff in op_events.c the description of events.
	for (uint i = 0 ; i < op_nr_events ; ++i) {
		if (op_events[i].cpu_mask & cpu_mask) {
			op_event_descr descr;

			descr.counter_mask = op_events[i].counter_mask;
			descr.val = op_events[i].val;
			if (op_events[i].unit) {
				descr.unit = &op_unit_masks[op_events[i].unit];
				descr.um_descr = &op_unit_descs[op_events[i].unit];
			} else {
				descr.unit = 0;
				descr.um_descr = 0;
			}

			descr.name = op_events[i].name;
			descr.help_str = op_event_descs[i];
			descr.min_count = op_events[i].min_count;

			for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
				if (!(descr.counter_mask & (1 << ctr)))
					continue;
 
				event_cfgs[ctr][descr.name].count = descr.min_count * 100;
				event_cfgs[ctr][descr.name].umask = 0;
				if (descr.unit)
					event_cfgs[ctr][descr.name].umask = descr.unit->default_mask;
				event_cfgs[ctr][descr.name].os_ring_count = 1;
				event_cfgs[ctr][descr.name].user_ring_count = 1;
			}

			v_events.push_back(descr);
		}
	}

	validate_buffer_size->setRange(OP_MIN_BUFFER_SIZE, OP_MAX_BUFFER_SIZE);
	validate_hash_table_size->setRange(OP_MIN_HASH_TABLE_SIZE, 
					   OP_MAX_HASH_TABLE_SIZE);

	validate_pid_filter->setRange(OP_MIN_PID, OP_MAX_PID);
	validate_pgrp_filter->setRange(OP_MIN_PID, OP_MAX_PID);

	events_list->setSorting(-1);
 
	for (uint i = 0; i < op_nr_counters; ++i) {
		counter_combo->insertItem("");
		set_counter_combo(i);
	}

	counter_selected(0);
	enabled_toggled(false);
 
	load_event_config_file();
}

// load the configuration, if the configuration file does not exist create it.
// the parent directory of the config file is created if necessary through
// save_config_file().
void oprof_start::load_config_file()
{
	std::string name = get_user_filename(".oprofile/oprof_start_config");

	{
		std::ifstream in(name.c_str());
		// this creates the config directory if necessary
		if (!in)
			save_config_file();
	}

	std::ifstream in(name.c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_config");
		return;
	}

	in >> config;
}

// save the configuration by overwritting the configuration file if it exist or
// create it. The parent directory of the config file is created if necessary
bool oprof_start::save_config_file()
{
	if (check_and_create_config_dir() == false)
		return false;

	std::string name = get_user_filename(".oprofile/oprof_start_config");

	std::ofstream out(name.c_str());
	
	out << config;

	return true;
}

// this work as load_config_file()/save_config_file()
void oprof_start::load_event_config_file()
{
	std::string name = get_user_filename(".oprofile/oprof_start_event");

	{
		std::ifstream in(name.c_str());
		// this creates the config directory if necessary
		if (!in)
			save_event_config_file();
	}

	std::ifstream in(name.c_str());
	if (!in) {
		QMessageBox::warning(this, 0, "Unable to open configuration "
				     "~/.oprofile/oprof_start_event");
		return;
	}

	// TODO: need checking on the key validity :(
	// FIXME: this needs to be per-counter 
	in >> event_cfgs[0];

	event_cfgs[0].set_dirty(false);
}

// this work as load_config_file()/save_config_file()
bool oprof_start::save_event_config_file()
{
	if (check_and_create_config_dir() == false)
		return false;

	std::string name = get_user_filename(".oprofile/oprof_start_event");

	std::ofstream out(name.c_str());

	// FIXME: !!!!! 
	out << event_cfgs[0];

	event_cfgs[0].set_dirty(false);

	return true;
}

// user is happy and want to quit in the normal way, so save the config file.
void oprof_start::accept()
{
	// record the previous settings
	record_selected_event_config();

	// TODO: check and warn about return code.
	// FIXME: counters
	if (event_cfgs[0].dirty()) {
		printf("saving configuration file");
		save_event_config_file();
	}

	record_config();

	save_config_file();

	QDialog::accept();
}

 
void oprof_start::set_counter_combo(uint ctr)
{
	std::string ctrstr("Counter ");
	char c = '0' + ctr;
	ctrstr += c;
	ctrstr += string(": ");
	if (current_event[ctr])
		ctrstr += current_event[ctr]->name;
	else
		ctrstr += "not used";
	counter_combo->changeItem(ctrstr.c_str(), ctr);
	counter_combo->setMinimumSize(counter_combo->sizeHint()); 
}

 
void oprof_start::counter_selected(int ctr)
{
	// record the previous settings
	record_selected_event_config();
 
	current_ctr = ctr;
 
	setUpdatesEnabled(false); 
	events_list->clear();
 
	for (std::vector<op_event_descr>::reverse_iterator cit = v_events.rbegin();
		cit != v_events.rend(); ++cit) {
 
		if (cit->counter_mask & (1 << ctr)) {
			QListViewItem * item = new QListViewItem(events_list, cit->name.c_str());
			if (current_event[ctr] != 0 && cit->name == current_event[ctr]->name)
				events_list->setCurrentItem(item);
		}
	}

	enabled->setChecked(ctr_enabled[ctr]);
 
	setUpdatesEnabled(true);
	update();
}

 
void oprof_start::event_selected(QListViewItem * item)
{
	// record the previous settings
	record_selected_event_config();
 
	op_event_descr const & descr = locate_event(item->text(0).latin1());

	setUpdatesEnabled(false); 
 
	event_help_label->setText(descr.help_str.c_str());
	setup_unit_masks(descr);
 
	validate_event_count->setRange(descr.min_count, OP_MAX_PERF_COUNT);
 
	const persistent_config_t<event_setting> & cfg = event_cfgs[current_ctr];
 
	os_ring_count_cb->setChecked(cfg[descr.name].os_ring_count);
	user_ring_count_cb->setChecked(cfg[descr.name].user_ring_count);
	QString count_text;
	count_text.setNum(cfg[descr.name].count);
	event_count_edit->setText(count_text);

	current_event[current_ctr] = &descr;
	set_counter_combo(current_ctr);
 
	setUpdatesEnabled(true);
	update();
}
 
 
void oprof_start::event_over(QListViewItem * item)
{
	op_event_descr const & descr = locate_event(item->text(0).latin1());
	event_help_label->setText(descr.help_str.c_str());
}

 
void oprof_start::enabled_toggled(bool en)
{
	ctr_enabled[current_ctr] = en;
	if (!en)
		events_list->clearSelection();
	events_list->setEnabled(en); 
	check0->setEnabled(en); 
	check1->setEnabled(en); 
	check2->setEnabled(en); 
	check3->setEnabled(en); 
	check4->setEnabled(en); 
	check5->setEnabled(en); 
	check6->setEnabled(en); 
	os_ring_count_cb->setEnabled(en); 
	user_ring_count_cb->setEnabled(en); 
	event_count_edit->setEnabled(en);
	set_counter_combo(current_ctr); 
}


// user need to select a file or directory, distinction about what the user
// needs to select is made through the source of the qt event.
void oprof_start::on_choose_file_or_dir()
{
	const QObject* source = sender();

	/* FIXME: yuck. let's just have separate slots for each event */
	if (source) {
		bool dir_only = false;
		QString base_dir;

		if (source->name() == QString("base_opd_dir_tb") ||
		    source->name() == QString("samples_files_dir_tb")) {
			dir_only = true;
			base_dir = base_opd_dir_edit->text();
		} else if (source->name() == QString("kernel_filename_tb")) {
			// need a stl to qt adapter?
			std::string result;
			std::string name=kernel_filename_edit->text().latin1();
			result = basename(name);
			base_dir = name.c_str();
		} else if (source->name() == QString("map_filename_tb")) {
			// need a stl to qt adapter?
			std::string result;
			std::string name = map_filename_edit->text().latin1();
			result = basename(name);
			base_dir = name.c_str();
		} else {
			base_dir = base_opd_dir_edit->text();
		}
		
		// the association between a file open tool button and the
		// edit widget is made through its name base on the naming
		// convention: object_name_tb --> object_name_edit
		QString result = do_open_file_or_dir(base_dir, dir_only);
		if (result.length()) {
			std::string src_name(source->name());
			// we support only '_tb' suffix, the right way is to
			// remove the '_' suffix and append the '_edit' suffix
			src_name = src_name.substr(0, src_name.length() - 3);
			src_name += "_edit";

			QObject* edit = child(src_name.c_str(), "QLineEdit");
			if (edit) {
				reinterpret_cast<QLineEdit*>(edit)->setText(result);
			}
		}
	} else {
		// Impossible if the dialog is well designed, if you see this
		// message you must make something sensible if source == 0
		fprintf(stderr, "oprof_start::on_choose_file_or_dir() slot is"
			"not directly call-able\n");
	}
}

// this record the curernt selected event setting in the event_cfg[] stuff.
// event_cfg->dirty is set only if change is recorded.
// TODO: need validation?
void oprof_start::record_selected_event_config()
{
	// saving must be made only if a change occur to avoid setting the
	// dirty flag in event_cfg. For the same reason we can not use a
	// temporay to an 'event_setting&' to clarify the code.
	// This come from:
	// if (event_cfg[name].xxx == yyyy) 
	// call the non const operator [] ofevent_cfg. We can probably do
	// better in event_cfg stuff but it need a proxy return from []. 
	// See persistent_config_t doc

	struct op_event_descr const * curr = current_event[current_ctr];

	if (!curr)
		return;
 
	const persistent_config_t<event_setting>& cfg = event_cfgs[current_ctr];
	std::string name(curr->name);

	uint count = event_count_edit->text().toUInt();

	if (cfg[name].count != count)
		event_cfgs[current_ctr][name].count = count;

	if (cfg[name].os_ring_count != os_ring_count_cb->isChecked())
		event_cfgs[current_ctr][name].os_ring_count = os_ring_count_cb->isChecked();
	
	if (cfg[name].user_ring_count != user_ring_count_cb->isChecked())
		event_cfgs[current_ctr][name].user_ring_count = user_ring_count_cb->isChecked();

	if (cfg[name].umask != get_unit_mask(*curr))
		event_cfgs[current_ctr][name].umask = get_unit_mask(*curr);
}

// same design as record_selected_event_config without trying to not dirties
// the config.dirty() flag, so config is always saved when user quit. This
// function also validate the result (The actual qt validator installed
// are not sufficient to do the validation)
bool oprof_start::record_config()
{
	config.kernel_filename = kernel_filename_edit->text().latin1();
	config.map_filename = map_filename_edit->text().latin1();
	
	if (config.buffer_size < OP_MIN_BUFFER_SIZE || 
	    config.buffer_size > OP_MAX_BUFFER_SIZE) {
		std::ostringstream error;

		error << "buffer size out of range: " << config.buffer_size
		      << " valid range is [" << OP_MIN_BUFFER_SIZE << ", "
		      << OP_MAX_BUFFER_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}

	config.buffer_size = buffer_size_edit->text().toUInt();

	if (config.hash_table_size < OP_MIN_HASH_TABLE_SIZE || 
	    config.hash_table_size > OP_MAX_HASH_TABLE_SIZE) {
		std::ostringstream error;

		error << "hash table size out of range: " 
		      << config.hash_table_size
		      << " valid range is [" << OP_MIN_HASH_TABLE_SIZE << ", "
		      << OP_MAX_HASH_TABLE_SIZE << "]";

		QMessageBox::warning(this, 0, error.str().c_str());

		return false;
	}

	config.hash_table_size = hash_table_size_edit->text().toUInt();
	config.pid_filter = pid_filter_edit->text().toUInt();
	config.pgrp_filter = pgrp_filter_edit->text().toUInt();
	config.base_opd_dir = base_opd_dir_edit->text().latin1();
	config.ignore_daemon_samples = ignore_daemon_samples_cb->isChecked();
	config.kernel_only = kernel_only_cb->isChecked();

	if (config.base_opd_dir.length() && 
	    config.base_opd_dir[config.base_opd_dir.length()-1] != '/')
		config.base_opd_dir += '/';

	return true;
}

void oprof_start::get_unit_mask_part(op_event_descr const & descr, uint num, bool selected, uint & mask)
{
	if (!selected)
		return; 
	if  (num >= descr.unit->num)
		return;
 
	if (descr.unit->unit_type_mask == utm_bitmask)
		mask |= descr.unit->um[num];
	else
		mask = descr.unit->um[num];
}

// return the unit mask selected through the unit mask check box
uint oprof_start::get_unit_mask(op_event_descr const & descr)
{
	// FIXME: utm_mandatory ?? 
 
	uint mask = 0;

	if (!descr.unit)
		return 0;
 
	get_unit_mask_part(descr, 0, check0->isChecked(), mask);
	get_unit_mask_part(descr, 1, check1->isChecked(), mask);
	get_unit_mask_part(descr, 2, check2->isChecked(), mask);
	get_unit_mask_part(descr, 3, check3->isChecked(), mask);
	get_unit_mask_part(descr, 4, check4->isChecked(), mask);
	get_unit_mask_part(descr, 5, check5->isChecked(), mask);
	get_unit_mask_part(descr, 6, check6->isChecked(), mask);
	return mask;
}

void oprof_start::setup_unit_masks(const op_event_descr & descr)
{
	// FIXME: the stuff needs rationalising between calling things "desc"
	// and "descr" 
	const op_unit_mask* um = descr.unit;
	const op_unit_desc* um_desc = descr.um_descr;

	check0->hide();
	check1->hide();
	check2->hide();
	check3->hide();
	check4->hide();
	check5->hide();
	check6->hide(); 
 
	if (!um || um->unit_type_mask == utm_mandatory)
		return;

	// we need const access. see record_selected_event_config()
	const persistent_config_t<event_setting>& cfg = event_cfgs[current_ctr];

	unit_mask_group->setExclusive(um->unit_type_mask == utm_exclusive);

	for (size_t i = 0; i < um->num ; ++i) {
		QCheckBox * check = 0;
		switch (i) {
			case 0: check = check0; break;
			case 1: check = check1; break;
			case 2: check = check2; break;
			case 3: check = check3; break;
			case 4: check = check4; break;
			case 5: check = check5; break;
			case 6: check = check6; break;
		}
		check->setText(um_desc->desc[i]);
		if (um->unit_type_mask == utm_exclusive) {
			check->setChecked(cfg[descr.name].umask == um->um[i]);
		} else {
			// FIXME: eh ?
			if (i == um->num - 1) {
				check->setChecked(cfg[descr.name].umask == um->um[i]);
			} else {
				check->setChecked(cfg[descr.name].umask & um->um[i]);
			}
		}
		check->show();
	}
}

void oprof_start::on_flush_profiler_data()
{
	if (is_profiler_started())
		do_exec_command("op_dump");
	else
		QMessageBox::warning(this, 0, "The profiler is not started.");
}

// user is happy of its setting.
void oprof_start::on_start_profiler()
{
	// save the current settings 
	record_selected_event_config();

	uint c; 
	for (c = 0; c < op_nr_counters; ++c) {
		if (ctr_enabled[c] && current_event[c])
			break;
	}
	if (c == op_nr_counters) {
		QMessageBox::warning(this, 0, "No counters enabled.\n");
		return;
	}
 
	for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
		if (!current_event[ctr])
			continue;
		if (!ctr_enabled[ctr])
			continue;

		// we need const access. see record_selected_event_config()
		const persistent_config_t<event_setting>& cfg = event_cfgs[ctr];

		const op_event_descr * descr = current_event[ctr];

		if (!cfg[descr->name].os_ring_count &&
		    !cfg[descr->name].user_ring_count) {
			QMessageBox::warning(this, 0, "You must select to "
					 "profile at least one of user binaries/kernel");
			return;
		}

		if (cfg[descr->name].count < descr->min_count ||
		    cfg[descr->name].count > OP_MAX_PERF_COUNT) {
			std::ostringstream out;

			out << "event " << descr->name << " count of range: "
			    << cfg[descr->name].count << " must be in [ "
			    << descr->min_count << ", "
			    << OP_MAX_PERF_COUNT
			    << "]";

			QMessageBox::warning(this, 0, out.str().c_str());
			return;
		}

		if (descr->unit && 
		    descr->unit->unit_type_mask != utm_exclusive &&
		    cfg[descr->name].umask == 0) {
			std::ostringstream out;

			out << "event " << descr->name<< " invalid unit mask: "
			    << cfg[descr->name].umask << std::endl;

			QMessageBox::warning(this, 0, out.str().c_str());
			return;
		}
	}

	if (is_profiler_started()) {
		int user_choice = 
			QMessageBox::warning(this, 0, 
					     "Profiler already started:\n\n"
					     "stop and restart it?", 
					     "&Restart", "&Cancel", 0, 0, 1);

		if (user_choice == 1)
			return;

		// this flush profiler data also.
		on_stop_profiler();
	}

	// record_config validate the config
	if (record_config() == false)
		return;

	// build the cmd line.
	std::ostringstream cmd_line;

	cmd_line << "op_start";

	for (uint ctr = 0; ctr < op_nr_counters; ++ctr) {
		if (!current_event[ctr])
			continue;
		if (!ctr_enabled[ctr])
			continue;

		// we need const access. see record_selected_event_config()
		const persistent_config_t<event_setting>& cfg = event_cfgs[ctr];

		const op_event_descr * descr = current_event[ctr];

		cmd_line << " --ctr" << ctr << "-event=" << descr->name;
		cmd_line << " --ctr" << ctr << "-count=" << cfg[descr->name].count;
		cmd_line << " --ctr" << ctr << "-kernel=" << cfg[descr->name].os_ring_count;
		cmd_line << " --ctr" << ctr << "-user=" << cfg[descr->name].user_ring_count;

		if (descr->um_descr)
			cmd_line << " --ctr" << ctr << "-unit-mask=" << cfg[descr->name].umask;
	}

	cmd_line << " --map-file=" << config.map_filename;
	cmd_line << " --vmlinux=" << config.kernel_filename;
	cmd_line << " --kernel-only=" << config.kernel_only;
	cmd_line << " --pid-filter=" << config.pid_filter;
	cmd_line << " --pgrp-filter=" << config.pgrp_filter;
	cmd_line << " --base-dir=" << config.base_opd_dir;
	cmd_line << " --samples-dir=" << config.base_opd_dir << config.samples_files_dir;
	cmd_line << " --device-file=" << config.base_opd_dir << config.device_file;
	cmd_line << " --hash-map-device-file=" << config.base_opd_dir << config.hash_map_device;
	cmd_line << " --log-file=" << config.base_opd_dir << config.daemon_log_file;
	cmd_line << " --ignore-myself=" << config.ignore_daemon_samples;
	cmd_line << " --buffer-size=" << config.buffer_size;
	cmd_line << " --hash-table-size=" << config.hash_table_size;

	std::cout << cmd_line.str() << std::endl;

	do_exec_command(cmd_line.str());
}

// flush and stop the profiler if it was started.
void oprof_start::on_stop_profiler()
{
	if (is_profiler_started()) {
		if (do_exec_command("op_dump") == 0) {
			do_exec_command("op_stop");
		}
	} else {
		QMessageBox::warning(this, 0, "The profiler is already stopped.");
	}
}

// helper to retrieve an event descr through its name.
const op_event_descr & oprof_start::locate_event(std::string const & name)
{
	// FIXME: use std::find_if
	for (size_t i = 0 ; i < v_events.size() ; ++i) {
		if (std::string(v_events[i].name) == name) {
			return v_events[i];
		}
	}
	return v_events[0];
}
