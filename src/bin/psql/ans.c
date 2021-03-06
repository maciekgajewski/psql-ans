/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2013, PostgreSQL Global Development Group
 *
 * src/bin/psql/ans.c
 */
#include "postgres_fe.h"

#include "ans.h"
#include "stringutils.h"

#include "postgres_fe.h"

static int global_ans_num = 0;

static void CreateTable(PGconn *db, struct _ans* item);
static char* GetTypeName(PGconn *db, Oid oid);
static char* BuildData(PGresult* result);

/*
 * Creates empty entry, serving as list head
 */
AnsHistory CreateAnsHistory(void)
{
	struct _ans* head;
	
	head = pg_malloc(sizeof(struct _ans));
	
	head->numColumns = 0;
	head->columnTypes = NULL;
	head->columnNames = NULL;
	head->data = NULL;
	head->next = NULL;
	head->tableName = NULL;
	head->name = NULL;
	
	return head;
}

void
AddToHistory(AnsHistory history, PGresult* result)
{
	struct _ans* item;
	int numRows, numColumns;
	int i;
	char* data;
	
	if (!history || !result)
		return;
	
	numColumns  = PQnfields(result);
	numRows = PQntuples(result);
	
	if (numColumns <= 0 || numRows <= 0)
		return;
	
	data = BuildData(result);
	if (!data)
		return;
	
	item = pg_malloc(sizeof(struct _ans));

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
	}
	
	item->data = data;
	
	/* Table creation deferred to when the ASN is actually used */
	item->tableName = NULL;

	/* name */
	item->name = pg_malloc(10);
	sprintf(item->name, "ans%d", global_ans_num++);
	
	printf(_("Query result stored as :%s\n"), item->name);
	
	item->next = history->next;
	history->next = item;
}

const char*
GetOrCreateTable(AnsHistory history, PGconn *db, const char* name)
{
	struct _ans* item;
	
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
			else
				return NULL;
		}
		item = item->next;
	}
	
	return NULL;
}

static
void 
CreateTable(PGconn *db, struct _ans* item)
{
	char tableNameBuf[32];
	char copyQuery[64];
	const int qbufsize=2048;
	char queryBuf[qbufsize];
	char* queryPtr;
	int len;
	int i;
	PGresult* qres;
	
	if (!item)
		return;
	
	len = snprintf(tableNameBuf, 32, "_table_%s", item->name);
	if (len >= 32)
	{
		return;
	}
	len = snprintf(copyQuery, 64, "COPY %s FROM STDIN", tableNameBuf);
	if (len >= 64)
	{
		return;
	}
	
	/* Build CREATE TABLE query */
	queryPtr = queryBuf;
	queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), "CREATE TEMP TABLE %s (\n", tableNameBuf);
	
	for (i = 0; i < item->numColumns; ++i)
	{
		char* typeName;
		const char* format;
		
		typeName = GetTypeName(db, item->columnTypes[i]);
		if (!typeName)
			return;
		
		if (i == item->numColumns-1)
			format = "%s %s\n";
		else
			format = "%s %s,\n";
			
		queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), format, item->columnNames[i], typeName);
		free(typeName);
		if (queryPtr >= queryBuf+qbufsize)
		{
			return; 
		}
	}
	queryPtr += snprintf(queryPtr, qbufsize-(int)(queryPtr-queryBuf), ");");
	if (queryPtr >= queryBuf+qbufsize)
	{
		return; 
	}
	
	/* create and populate table */
	PQclear(PQexec(db, "BEGIN"));
	qres = PQexec(db, queryBuf);
	if (PQresultStatus(qres) != PGRES_COMMAND_OK)
	{
		PQclear(qres);
		PQclear(PQexec(db, "ROLLBACK"));
		return;
	}
	PQclear(qres);

	qres = PQexec(db, copyQuery);
	if (PQresultStatus(qres) != PGRES_COPY_IN)
	{
		PQclear(qres);
		PQclear(PQexec(db, "ROLLBACK"));
		return;
	}
	PQclear(qres);
	
	PQputCopyData(db, item->data, strlen(item->data));
	len = PQputCopyEnd(db, NULL);
	if (len != 1)
	{
		PQclear(PQexec(db, "ROLLBACK"));
		return;
	}
	qres = PQgetResult(db);
	if (PQresultStatus(qres) != PGRES_COMMAND_OK)
	{
		PQclear(qres);
		PQclear(PQexec(db, "ROLLBACK"));
		return;
	}
	PQclear(qres);
	PQclear(PQexec(db, "COMMIT"));
	
	item->tableName = pg_strdup(tableNameBuf);
}

/* free the typename after use */
static
char* 
GetTypeName(PGconn *db, Oid oid)
{
	const int bufsize = 1024;
	char queryBuf[bufsize];
	int sres;
	PGresult* qres;
	char* typeName;
	
	sres = snprintf(queryBuf, bufsize, "SELECT typname FROM pg_type WHERE oid=%d;", oid);
	if ( sres < 0 || sres > bufsize)
		return NULL;
	
	qres = PQexec(db, queryBuf);
	if (!qres)
		return NULL;
	
	if (PQresultStatus(qres) != PGRES_TUPLES_OK)
	{
		PQclear(qres);
		return NULL;
	}
	
	if (PQntuples(qres) != 1 || PQnfields(qres) != 1)
	{
		PQclear(qres);
		return NULL;
	}

	typeName = pg_strdup(PQgetvalue(qres, 0, 0));
	PQclear(qres);
	return typeName;
}

/* Convert query data into data buffer in COPY TEXT format.
 * returns NULL on error
 * caller owns the data
 */
static 
char* 
BuildData(PGresult* result)
{
	int cols, rows;
	int bufferSize;
	int r,c;
	char* buffer;
	char* dataPtr;
	
	if (!result)
		return NULL;
	
	cols = PQnfields(result);
	rows = PQntuples(result);
	
	/* first pass - measure the size */
	bufferSize = 0;
	for(r = 0; r < rows; r++)
	{
		for(c = 0; c < cols; c++)
		{
			if (PQgetisnull(result, r, c))
			{
				bufferSize += 2; /* 2 = size of null literal \N */
			}
			else
			{
				bufferSize += get_escaped_for_copy_len(PQgetvalue(result, r, c));
			}
			bufferSize +=1; /* column or row delimiter*/
		}
	}
	bufferSize += 4; /* end-of-data marker \. + NULL terminiator */
	
	/* second pass = build the buffer */
	buffer = pg_malloc(bufferSize);
	dataPtr = buffer;
	
	for(r = 0; r < rows; r++)
	{
		for(c = 0; c < cols; c++)
		{
			if (PQgetisnull(result, r, c))
			{
				strcpy(dataPtr, "\\N");
				dataPtr += 2;
			}
			else
			{
				char* value = PQgetvalue(result, r, c);
				dataPtr = escape_for_copy(dataPtr, value);
			}
			if (c == cols-1)
			{
				strcpy(dataPtr, "\n");
			}
			else
			{
				strcpy(dataPtr, "\t");
			}
			dataPtr += 1;
		}
	}
	strcpy(dataPtr, "\\.\n");
	
	return buffer;
}

void AnsClearTableNames(AnsHistory history)
{
	struct _ans* item = history;
	
	while(item->next)
	{
		item = item->next;
		
		if (item->tableName)
		{
			free(item->tableName);
			item->tableName = NULL;
		}
	}	
}

void
DestroyAnsHistory(PGconn *db, AnsHistory item)
{
	if (!item)
		return;
	
	if (item->next)
		DestroyAnsHistory(db, item->next);
	
	if (item->data)
		free(item->data);
	
	if(item->name)
		free(item->name);
	
	if (item->columnTypes)
		free(item->columnTypes);
	
	if (item->numColumns && item->columnNames)
	{
		int i;
		for(i = 0; i < item->numColumns; i++)
			free(item->columnNames[i]);
		
		free(item->columnNames);
	}
	
	if (item->tableName)
	{
		const int bufsize = 32;
		char deleteQuery[bufsize];

		snprintf(deleteQuery, bufsize, "DROP TABLE %s", item->tableName);
		PQclear(PQexec(db, deleteQuery));
	}
	
	free(item);
}
