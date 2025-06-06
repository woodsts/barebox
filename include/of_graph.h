/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * OF graph binding parsing helpers
 *
 * Copyright (C) 2012 - 2013 Samsung Electronics Co., Ltd.
 * Author: Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * Copyright (C) 2012 Renesas Electronics Corp.
 * Author: Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 */
#ifndef __LINUX_OF_GRAPH_H
#define __LINUX_OF_GRAPH_H

/**
 * struct of_endpoint - the OF graph endpoint data structure
 * @port: identifier (value of reg property) of a port this endpoint belongs to
 * @id: identifier (value of reg property) of this endpoint
 * @local_node: pointer to device_node of this endpoint
 */
struct of_endpoint {
	unsigned int port;
	unsigned int id;
	const struct device_node *local_node;
};

/**
 * for_each_endpoint_of_node - iterate over every endpoint in a device node
 * @parent: parent device node containing ports and endpoints
 * @child: loop variable pointing to the current endpoint node
 *
 * When breaking out of the loop, of_node_put(child) has to be called manually.
 */
#define for_each_endpoint_of_node(parent, child) \
	for (child = of_graph_get_next_endpoint(parent, NULL); child != NULL; \
	     child = of_graph_get_next_endpoint(parent, child))

int of_graph_parse_endpoint(const struct device_node *node,
				struct of_endpoint *endpoint);
struct device_node *of_graph_get_port_by_id(struct device_node *node, u32 id);
struct device_node *of_graph_get_next_endpoint(const struct device_node *parent,
					struct device_node *previous);
struct device_node *of_graph_get_remote_port_parent(
					const struct device_node *node);
struct device_node *of_graph_get_remote_port(const struct device_node *node);
int of_graph_port_is_available(struct device_node *node);
struct device_node *of_graph_get_remote_node(const struct device_node *node,
					     u32 port, u32 endpoint);
struct device_node *of_graph_get_endpoint_by_regs(
	const struct device_node *parent, int port_reg, int reg);

#endif /* __LINUX_OF_GRAPH_H */
