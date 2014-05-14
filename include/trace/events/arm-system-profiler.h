/*
 * ARM System Profiler trace events
 *
 * Copyright (C) 2014 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if !defined(ARM_SYSTEM_PROFILER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define ARM_SYSTEM_PROFILER_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(arm_system_profiler_capture,
	TP_PROTO(struct system_profiler_desc const *sp,
		 struct port_desc const *port),
	TP_ARGS(sp, port),
	TP_STRUCT__entry(
		__string(profiler, sp->pdev->name)
		__field(u32, serial)
		__field(s32, port_id)
		__string(port_type, port->type->name)
		__string(port_target, port->description)
		__field(u32, data_count)
		__dynamic_array(u32, data, port->type->num_capture_regs)
	),
	TP_fast_assign(
		strcpy(__get_str(profiler), sp->pdev->name);
		__entry->serial = sp->serial;
		__entry->port_id = port->id;
		__assign_str(port_type, port->type->name);
		__assign_str(port_target, port->description);
		__entry->data_count = port->type->num_capture_regs;
		capture_port(sp, port, __get_dynamic_array(data),
			     __entry->data_count);
	),
	TP_printk("profiler=\"%s\", serial=%u, port_id=%d, port_type=\"%s\", port_target=\"%s\", data_count=%u, data=%s",
		__get_str(profiler),
		(unsigned int)__entry->serial,
		(unsigned int)__entry->port_id,
		__get_str(port_type),
		__get_str(port_target),
		(unsigned int)__entry->data_count,
		arm_system_profiler_print_u32_array(p,
						    __get_dynamic_array(data),
						    __entry->data_count)
	)
)

#endif /* ! ARM_SYSTEM_PROFILER_TRACE_H */

#define TRACE_INCLUDE_FILE arm-system-profiler
#include <trace/define_trace.h>
