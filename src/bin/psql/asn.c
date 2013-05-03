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

static void CreateTable(PGconn *db, struct _asn* item);
static const char* GetTypeName(PGconn *db, Oid oid);

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
GetOrCreateTable(AsnHistory history, PGconn *db, const char* name)
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
			
			CreateTable(db, item);
			if (item->tableName)
				return item->tableName;
		}
	}
	
	return NULL;
}

static
void 
CreateTable(PGconn *db, struct _asn* item)
{
	char tableNameBuf[32];
	const int qbufsize=1024;
	char queryBuf[qbufsize];
	char* queryPtr;
	int nameLen;
	int i;
	
	if (!item)
		return;
	
	nameLen = snprintf(tableNameBuf, 32, "_table_%s", item->name);
	
	/* Build CREATE TABLE query */
	queryPtr = queryBuf;
	queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), "CREATE TEMP TABLE %s (\n", tableNameBuf);
	
	for (i = 0; i < item->numColumns; ++i)
	{
		const char* typeName;
		const char* format;
		
		typeName = GetTypeName(db, item->columnTypes[i]);
		if (!typeName)
			return;
		
		if (i == item->numColumns-1)
			format = "%s %s\n";
		else
			format = "%s %s,\n";
			
		queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), format, item->columnNames[i], typeName);
	}
	queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), ");");
	
	/* see if we did'n overrun buffer */
	if (queryPtr == queryBuf+qbufsize)
	{
		printf(">>>AND CREATE TABLE query buffer overrun\n");
		return; 
	}
	
	/* execute query */
	// TODO
	printf(">>>ANS: now i should execute: \n%s\n", queryBuf);
	
	item->tableName = pg_strdup(tableNameBuf);
}

static
const char* 
GetTypeName(PGconn *db, Oid oid)
{
	// TODO
	return "integer";
}
