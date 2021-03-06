/*
 * psql - the PostgreSQL interactive terminal
 *
 * Copyright (c) 2013, PostgreSQL Global Development Group
 *
 * src/bin/psql/ans.h
 */
#ifndef ANS_H
#define ANS_H

#include "libpq-fe.h"


/* Cache of last N query results, stored in a format ready to insert itno tempary table when needed */
struct _ans 
{
	int		numColumns;
	Oid		*columnTypes;
	char	**columnNames;
	char	*data;
	
	char	*name; // history item name
	char	*tableName; // corresponding table in DB. NULL if not created yet
	
	struct _ans	*next;
};

typedef struct _ans* AnsHistory;

AnsHistory	CreateAnsHistory(void);
void		AddToHistory(AnsHistory history, PGresult* result);
void		DestroyAnsHistory(PGconn *db, AnsHistory item);


/*
 * Takes variable name and checks wether it matches the name of existing history item.
 * If none found, returns NULL.
 * If found, but table not created yet, creates the table first.
 */
const char* GetOrCreateTable(AnsHistory history, PGconn *db, const char* name);

/* Should be called when new connection is open.
 * Because all the tables used by ANS are temporary and kept in current DB, thety are no longer valid
 * after the client connect to new database
 */
void AnsClearTableNames(AnsHistory history);

#endif
