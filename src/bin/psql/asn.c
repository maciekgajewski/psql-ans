/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2013, PostgreSQL Global Development Group
 *
 * src/bin/psql/asn.c
 */

#include "asn.h"

#include "postgres_fe.h"

static int global_asn_num = 0;

static void CreateTable(struct _asn* item);

/*
 * Creates empty entry, serving as list head
 */
AsnHistory CreateAsnHistory(void)
{
	struct _asn* head;
	
	head = pg_malloc(sizeof(struct _asn));
	
	head->numColumns = 0;
	head->columnTypes = NULL;
	head->columnNames = NULL;
	head->data = NULL;
	head->next = NULL;
	head->tableName = NULL;
	
	return head;
}

void
AddToHistory(AsnHistory history, PGresult* result)
{
	struct _asn* item;
	int numRows, numColumns;
	int i;
	
	if (!history || !result)
		return;
	
	numColumns  = PQnfields(result);
	numRows = PQntuples(result);
	
	if (numColumns <= 0 || numRows <= 0)
		return;
	
	item = pg_malloc(sizeof(struct _asn));

	/* read meta-data: column names and types */
	item->numColumns = numColumns;
	item->columnNames = pg_malloc(sizeof(char*) * item->numColumns);
	item->columnTypes = pg_malloc(sizeof(Oid) * item->numColumns);
	
	for (i = 0; i < item->numColumns; i++)
	{
		char* name;
		
		item->columnTypes[i] = PQftype(result, i);
		
		name = PQfname(result, i);
		item->columnNames[i] = pg_malloc(strlen(name) + 1);
		strcpy(item->columnNames[i], name);
		
		// TODO debug output
		//printf(">>> ASN column %d %s %d\n", i, name, (int)(item->columnTypes[i]));
	}
	
	/* TODO build data here */
	item->data = NULL;
	
	/* Table creation deferred to when the ASN is actually used */
	item->tableName = NULL;

	/* name */
	item->name = pg_malloc(10);
	sprintf(item->name, "asn%d", global_asn_num++);
	
	printf("Query result stored as :%s\n", item->name);
	
	item->next = history->next;
	history->next = item;
}

const char*
GetOrCreateTable(AsnHistory history, const char* name)
{
	struct _asn* item;
	
	if (!history || !name)
	{
		return NULL;
	}
	
	item = history->next;
	
	while (item)
	{
		if (strcmp(item->name, name) == 0)
		{
			if (item->tableName)
				return item->tableName;
			
			CreateTable(item);
			if (item->tableName)
				return item->tableName;
		}
	}
	
	return NULL;
}

static void CreateTable(struct _asn* item)
{
	char tableNameBuf[32];
	int nameLen;
	
	if (!item)
		return NULL;
	
	nameLen = snprintf(tableNameBuf, 32, "_table_%s", item->name);
	item->tableName = pg_malloc(nameLen+1);
	strcpy(item->tableName, tableNameBuf);
}
