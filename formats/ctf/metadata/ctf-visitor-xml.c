/*
 * ctf-visitor-xml.c
 *
 * Common Trace Format Metadata Visitor (XML dump).
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/list.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

#define fprintf_dbg(fd, fmt, args...)	fprintf(fd, "%s: " fmt, __func__, ## args)

static
void print_tabs(FILE *fd, int depth)
{
	int i;

	for (i = 0; i < depth; i++)
		fprintf(fd, "\t");
}

static
int ctf_visitor_print_unary_expression(FILE *fd, int depth, struct ctf_node *node)
{
	int ret = 0;

	switch (node->u.unary_expression.link) {
	case UNARY_LINK_UNKNOWN:
		break;
	case UNARY_DOTLINK:
		print_tabs(fd, depth);
		fprintf(fd, "<dotlink/>\n");
		break;
	case UNARY_ARROWLINK:
		print_tabs(fd, depth);
		fprintf(fd, "<arrowlink/>\n");
		break;
	case UNARY_DOTDOTDOT:
		print_tabs(fd, depth);
		fprintf(fd, "<dotdotdot/>\n");
		break;
	default:
		fprintf(stderr, "[error] %s: unknown expression link type %d\n", __func__,
			(int) node->u.unary_expression.link);
		return -EINVAL;
	}

	switch (node->u.unary_expression.type) {
	case UNARY_STRING:
		print_tabs(fd, depth);
		fprintf(fd, "<unary_expression value=");
		fprintf(fd, "\"%s\"", node->u.unary_expression.u.string);
		fprintf(fd, " />\n");
		break;
	case UNARY_SIGNED_CONSTANT:
		print_tabs(fd, depth);
		fprintf(fd, "<unary_expression value=");
		fprintf(fd, "%" PRId64, node->u.unary_expression.u.signed_constant);
		fprintf(fd, " />\n");
		break;
	case UNARY_UNSIGNED_CONSTANT:
		print_tabs(fd, depth);
		fprintf(fd, "<unary_expression value=");
		fprintf(fd, "%" PRIu64, node->u.unary_expression.u.signed_constant);
		fprintf(fd, " />\n");
		break;
	case UNARY_SBRAC:
		print_tabs(fd, depth);
		fprintf(fd, "<unary_expression_sbrac>\n");
		ret = ctf_visitor_print_unary_expression(fd, depth + 1,
			node->u.unary_expression.u.sbrac_exp);
		if (ret)
			return ret;
		print_tabs(fd, depth);
		fprintf(fd, "</unary_expression_sbrac>\n");
		break;
	case UNARY_NESTED:
		print_tabs(fd, depth);
		fprintf(fd, "<unary_expression_nested>\n");
		ret = ctf_visitor_print_unary_expression(fd, depth + 1,
			node->u.unary_expression.u.nested_exp);
		if (ret)
			return ret;
		print_tabs(fd, depth);
		fprintf(fd, "</unary_expression_nested>\n");
		break;

	case UNARY_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown expression type %d\n", __func__,
			(int) node->u.unary_expression.type);
		return -EINVAL;
	}
	return 0;
}

static
int ctf_visitor_print_type_specifier(FILE *fd, int depth, struct ctf_node *node)
{
	print_tabs(fd, depth);
	fprintf(fd, "<type_specifier \"");

	switch (node->u.type_specifier.type) {
	case TYPESPEC_VOID:
		fprintf(fd, "void");
		break;
	case TYPESPEC_CHAR:
		fprintf(fd, "char");
		break;
	case TYPESPEC_SHORT:
		fprintf(fd, "short");
		break;
	case TYPESPEC_INT:
		fprintf(fd, "int");
		break;
	case TYPESPEC_LONG:
		fprintf(fd, "long");
		break;
	case TYPESPEC_FLOAT:
		fprintf(fd, "float");
		break;
	case TYPESPEC_DOUBLE:
		fprintf(fd, "double");
		break;
	case TYPESPEC_SIGNED:
		fprintf(fd, "signed");
		break;
	case TYPESPEC_UNSIGNED:
		fprintf(fd, "unsigned");
		break;
	case TYPESPEC_BOOL:
		fprintf(fd, "bool");
		break;
	case TYPESPEC_COMPLEX:
		fprintf(fd, "complex");
		break;
	case TYPESPEC_CONST:
		fprintf(fd, "const");
		break;
	case TYPESPEC_ID_TYPE:
		fprintf(fd, "%s", node->u.type_specifier.id_type);
		break;

	case TYPESPEC_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown type specifier %d\n", __func__,
			(int) node->u.type_specifier.type);
		return -EINVAL;
	}
	fprintf(fd, "\"/>\n");
	return 0;
}

static
int ctf_visitor_print_type_declarator(FILE *fd, int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	print_tabs(fd, depth);
	fprintf(fd, "<type_declarator>\n");
	depth++;

	if (!cds_list_empty(&node->u.type_declarator.pointers)) {
		print_tabs(fd, depth);
		fprintf(fd, "<pointers>\n");
		cds_list_for_each_entry(iter, &node->u.type_declarator.pointers,
					siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</pointers>\n");
	}

	switch (node->u.type_declarator.type) {
	case TYPEDEC_ID:
		if (node->u.type_declarator.u.id) {
			print_tabs(fd, depth);
			fprintf(fd, "<id \"");
			fprintf(fd, "%s", node->u.type_declarator.u.id);
			fprintf(fd, "\" />\n");
		}
		break;
	case TYPEDEC_NESTED:
		if (node->u.type_declarator.u.nested.type_declarator) {
			print_tabs(fd, depth);
			fprintf(fd, "<type_declarator>\n");
			ret = ctf_visitor_print_xml(fd, depth + 1,
				node->u.type_declarator.u.nested.type_declarator);
			if (ret)
				return ret;
			print_tabs(fd, depth);
			fprintf(fd, "</type_declarator>\n");
		}
		if (node->u.type_declarator.u.nested.length) {
			print_tabs(fd, depth);
			fprintf(fd, "<length>\n");
			ret = ctf_visitor_print_xml(fd, depth + 1,
				node->u.type_declarator.u.nested.length);
			if (ret)
				return ret;
			print_tabs(fd, depth);
			fprintf(fd, "</length>\n");
		}
		if (node->u.type_declarator.u.nested.abstract_array) {
			print_tabs(fd, depth);
			fprintf(fd, "<length>\n");
			print_tabs(fd, depth);
			fprintf(fd, "</length>\n");
		}
		if (node->u.type_declarator.bitfield_len) {
			print_tabs(fd, depth);
			fprintf(fd, "<bitfield_len>\n");
			ret = ctf_visitor_print_xml(fd, depth + 1,
				node->u.type_declarator.bitfield_len);
			if (ret)
				return ret;
			print_tabs(fd, depth);
			fprintf(fd, "</bitfield_len>\n");
		}
		break;
	case TYPEDEC_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown type declarator %d\n", __func__,
			(int) node->u.type_declarator.type);
		return -EINVAL;
	}

	depth--;
	print_tabs(fd, depth);
	fprintf(fd, "</type_declarator>\n");
	return 0;
}

int ctf_visitor_print_xml(FILE *fd, int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	switch (node->type) {
	case NODE_ROOT:
		print_tabs(fd, depth);
		fprintf(fd, "<root>\n");
		cds_list_for_each_entry(iter, &node->u.root._typedef,
					siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.typealias,
					siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.declaration_specifier, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</root>\n");
		break;

	case NODE_EVENT:
		print_tabs(fd, depth);
		fprintf(fd, "<event>\n");
		cds_list_for_each_entry(iter, &node->u.event.declaration_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</event>\n");
		break;
	case NODE_STREAM:
		print_tabs(fd, depth);
		fprintf(fd, "<stream>\n");
		cds_list_for_each_entry(iter, &node->u.stream.declaration_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</stream>\n");
		break;
	case NODE_TRACE:
		print_tabs(fd, depth);
		fprintf(fd, "<trace>\n");
		cds_list_for_each_entry(iter, &node->u.trace.declaration_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</trace>\n");
		break;

	case NODE_CTF_EXPRESSION:
		print_tabs(fd, depth);
		fprintf(fd, "<ctf_expression>\n");
		depth++;
		print_tabs(fd, depth);
		fprintf(fd, "<left>\n");
		cds_list_for_each_entry(iter, &node->u.ctf_expression.left, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}

		print_tabs(fd, depth);
		fprintf(fd, "</left>\n");

		print_tabs(fd, depth);
		fprintf(fd, "<right>\n");
		cds_list_for_each_entry(iter, &node->u.ctf_expression.right, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</right>\n");
		depth--;
		print_tabs(fd, depth);
		fprintf(fd, "</ctf_expression>\n");
		break;
	case NODE_UNARY_EXPRESSION:
		return ctf_visitor_print_unary_expression(fd, depth, node);

	case NODE_TYPEDEF:
		print_tabs(fd, depth);
		fprintf(fd, "<typedef>\n");
		depth++;
		print_tabs(fd, depth);
		fprintf(fd, "<declaration_specifier>\n");
		cds_list_for_each_entry(iter, &node->u._typedef.declaration_specifier, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</declaration_specifier>\n");

		print_tabs(fd, depth);
		fprintf(fd, "<type_declarators>\n");
		cds_list_for_each_entry(iter, &node->u._typedef.type_declarators, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</type_declarators>\n");
		depth--;
		print_tabs(fd, depth);
		fprintf(fd, "</typedef>\n");
		break;
	case NODE_TYPEALIAS_TARGET:
		print_tabs(fd, depth);
		fprintf(fd, "<target>\n");
		depth++;

		print_tabs(fd, depth);
		fprintf(fd, "<declaration_specifier>\n");
		cds_list_for_each_entry(iter, &node->u.typealias_target.declaration_specifier, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</declaration_specifier>\n");

		print_tabs(fd, depth);
		fprintf(fd, "<type_declarators>\n");
		cds_list_for_each_entry(iter, &node->u.typealias_target.type_declarators, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</type_declarators>\n");

		depth--;
		print_tabs(fd, depth);
		fprintf(fd, "</target>\n");
		break;
	case NODE_TYPEALIAS_ALIAS:
		print_tabs(fd, depth);
		fprintf(fd, "<alias>\n");
		depth++;

		print_tabs(fd, depth);
		fprintf(fd, "<declaration_specifier>\n");
		cds_list_for_each_entry(iter, &node->u.typealias_alias.declaration_specifier, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</declaration_specifier>\n");

		print_tabs(fd, depth);
		fprintf(fd, "<type_declarators>\n");
		cds_list_for_each_entry(iter, &node->u.typealias_alias.type_declarators, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</type_declarators>\n");

		depth--;
		print_tabs(fd, depth);
		fprintf(fd, "</alias>\n");
		break;
	case NODE_TYPEALIAS:
		print_tabs(fd, depth);
		fprintf(fd, "<typealias>\n");
		ret = ctf_visitor_print_xml(fd, depth + 1, node->u.typealias.target);
		if (ret)
			return ret;
		ret = ctf_visitor_print_xml(fd, depth + 1, node->u.typealias.alias);
		if (ret)
			return ret;
		print_tabs(fd, depth);
		fprintf(fd, "</typealias>\n");
		break;

	case NODE_TYPE_SPECIFIER:
		ret = ctf_visitor_print_type_specifier(fd, depth, node);
		if (ret)
			return ret;
		break;
	case NODE_POINTER:
		print_tabs(fd, depth);
		if (node->u.pointer.const_qualifier)
			fprintf(fd, "<const_pointer />\n");
		else
			fprintf(fd, "<pointer />\n");
		break;
	case NODE_TYPE_DECLARATOR:
		ret = ctf_visitor_print_type_declarator(fd, depth, node);
		if (ret)
			return ret;
		break;

	case NODE_FLOATING_POINT:
		print_tabs(fd, depth);
		fprintf(fd, "<floating_point>\n");
		cds_list_for_each_entry(iter, &node->u.floating_point.expressions, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</floating_point>\n");
		break;
	case NODE_INTEGER:
		print_tabs(fd, depth);
		fprintf(fd, "<integer>\n");
		cds_list_for_each_entry(iter, &node->u.integer.expressions, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</integer>\n");
		break;
	case NODE_STRING:
		print_tabs(fd, depth);
		fprintf(fd, "<string>\n");
		cds_list_for_each_entry(iter, &node->u.string.expressions, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</string>\n");
		break;
	case NODE_ENUMERATOR:
		print_tabs(fd, depth);
		fprintf(fd, "<enumerator");
		if (node->u.enumerator.id)
			fprintf(fd, " id=\"%s\"", node->u.enumerator.id);
		fprintf(fd, ">\n");
		cds_list_for_each_entry(iter, &node->u.enumerator.values, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</enumerator>\n");
		break;
	case NODE_ENUM:
		print_tabs(fd, depth);
		if (node->u._struct.name)
			fprintf(fd, "<enum name=\"%s\">\n",
				node->u._enum.enum_id);
		else
			fprintf(fd, "<enum >\n");
		depth++;

		if (node->u._enum.container_type) {
			print_tabs(fd, depth);
			fprintf(fd, "<container_type>\n");
			ret = ctf_visitor_print_xml(fd, depth + 1, node->u._enum.container_type);
			if (ret)
				return ret;
			print_tabs(fd, depth);
			fprintf(fd, "</container_type>\n");
		}

		print_tabs(fd, depth);
		fprintf(fd, "<enumerator_list>\n");
		cds_list_for_each_entry(iter, &node->u._enum.enumerator_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</enumerator_list>\n");

		depth--;
		print_tabs(fd, depth);
		fprintf(fd, "</enum>\n");
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		print_tabs(fd, depth);
		fprintf(fd, "<declaration_specifier>\n");
		cds_list_for_each_entry(iter, &node->u.struct_or_variant_declaration.declaration_specifier, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</declaration_specifier>\n");

		print_tabs(fd, depth);
		fprintf(fd, "<type_declarators>\n");
		cds_list_for_each_entry(iter, &node->u.struct_or_variant_declaration.type_declarators, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</type_declarators>\n");
		break;
	case NODE_VARIANT:
		print_tabs(fd, depth);
		fprintf(fd, "<variant");
		if (node->u.variant.name)
			fprintf(fd, " name=\"%s\"", node->u.variant.name);
		if (node->u.variant.choice)
			fprintf(fd, " choice=\"%s\"", node->u.variant.choice);
		fprintf(fd, ">\n");
		cds_list_for_each_entry(iter, &node->u.variant.declaration_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</variant>\n");
		break;
	case NODE_STRUCT:
		print_tabs(fd, depth);
		if (node->u._struct.name)
			fprintf(fd, "<struct name=\"%s\">\n",
				node->u._struct.name);
		else
			fprintf(fd, "<struct>\n");
		cds_list_for_each_entry(iter, &node->u._struct.declaration_list, siblings) {
			ret = ctf_visitor_print_xml(fd, depth + 1, iter);
			if (ret)
				return ret;
		}
		print_tabs(fd, depth);
		fprintf(fd, "</struct>\n");
		break;

	case NODE_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown node type %d\n", __func__,
			(int) node->type);
		return -EINVAL;
	}
	return ret;
}
