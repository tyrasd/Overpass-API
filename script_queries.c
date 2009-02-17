#include <cctype>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <vector>
#include "file_types.h"
#include "script_datatypes.h"
#include "script_queries.h"
#include "user_interface.h"
#include "vigilance_control.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <mysql.h>

using namespace std;

MYSQL_RES* mysql_query_wrapper(MYSQL* mysql, string query)
{
  int query_status(mysql_query(mysql, query.c_str()));
  if (query_status)
  {
    ostringstream temp;
    temp<<"Error during SQL query ";
    temp<<'('<<query_status<<"):\n";
    temp<<"Query: "<<query<<'\n';
    temp<<"Error: "<<mysql_error(mysql)<<'\n';
    runtime_error(temp.str(), cout);
  }

  MYSQL_RES* result(mysql_store_result(mysql));
  if (!result)
  {
    if (is_timed_out())
      runtime_error("Your query timed out.", cout);
    ostringstream temp;
    temp<<"Error during SQL query (result is null pointer)\n";
    temp<<mysql_error(mysql)<<'\n';
    runtime_error(temp.str(), cout);
  }
  
  return result;
}

MYSQL_RES* mysql_query_use_wrapper(MYSQL* mysql, string query)
{
  int query_status(mysql_query(mysql, query.c_str()));
  if (query_status)
  {
    ostringstream temp;
    temp<<"Error during SQL query ";
    temp<<'('<<query_status<<"):\n";
    temp<<"Query: "<<query<<'\n';
    temp<<"Error: "<<mysql_error(mysql)<<'\n';
    runtime_error(temp.str(), cout);
  }

  MYSQL_RES* result(mysql_use_result(mysql));
  if (!result)
  {
    if (is_timed_out())
      runtime_error("Your query timed out.", cout);
    ostringstream temp;
    temp<<"Error during SQL query (result is null pointer)\n";
    temp<<mysql_error(mysql)<<'\n';
    runtime_error(temp.str(), cout);
  }
  
  return result;
}

void mysql_query_null_wrapper(MYSQL* mysql, string query)
{
  int query_status(mysql_query(mysql, query.c_str()));
  if (query_status)
  {
    ostringstream temp;
    temp<<"Error during SQL query ";
    temp<<'('<<query_status<<"):\n";
    temp<<"Query: "<<query<<'\n';
    temp<<"Error: "<<mysql_error(mysql)<<'\n';
    runtime_error(temp.str(), cout);
  }
}

int int_query(MYSQL* mysql, string query)
{
  int result_val(0);
  MYSQL_RES* result(mysql_query_use_wrapper(mysql, query));
  if (!result)
    return 0;
	
  MYSQL_ROW row(mysql_fetch_row(result));
  if ((row) && (row[0]))
    result_val = atoi(row[0]);
  
  while (mysql_fetch_row(result))
    ;
  mysql_free_result(result);
  return result_val;
}

pair< int, int > intint_query(MYSQL* mysql, string query)
{
  pair< int, int > result_val(make_pair< int, int >(0, 0));
  MYSQL_RES* result(mysql_query_use_wrapper(mysql, query));
  if (!result)
    return result_val;
	
  MYSQL_ROW row(mysql_fetch_row(result));
  if ((row) && (row[0]) && (row[1]))
    result_val = make_pair< int, int >(atoi(row[0]), atoi(row[1]));
  
  while (mysql_fetch_row(result))
    ;
  mysql_free_result(result);
  return result_val;
}

set< int >& multiint_query(MYSQL* mysql, string query, set< int >& result_set)
{
  MYSQL_RES* result(mysql_query_use_wrapper(mysql, query));
  if (!result)
    return result_set;
	
  MYSQL_ROW row(mysql_fetch_row(result));
  while ((row) && (row[0]))
  {
    result_set.insert(atoi(row[0]));
    row = mysql_fetch_row(result);
  }
  
  while (mysql_fetch_row(result))
    ;
  mysql_free_result(result);
  return result_set;
}

set< Node >& multiNode_query(MYSQL* mysql, string query, set< Node >& result_set)
{
  MYSQL_RES* result(mysql_query_use_wrapper(mysql, query));
  if (!result)
    return result_set;
	
  MYSQL_ROW row(mysql_fetch_row(result));
  while ((row) && (row[0]) && (row[1]) && (row[2]))
  {
    result_set.insert(Node(atoi(row[0]), atoi(row[1]), atoi(row[2])));
    row = mysql_fetch_row(result);
  }
  
  while (mysql_fetch_row(result))
    ;
  mysql_free_result(result);
  return result_set;
}

set< Area >& multiArea_query(MYSQL* mysql, string query, int lat, int lon, set< Area >& result_set)
{
  MYSQL_RES* result(mysql_query_use_wrapper(mysql, query));
  if (!result)
    return result_set;
	
  map< int, bool > area_cands;
  set< int > area_definitives;
  MYSQL_ROW row(mysql_fetch_row(result));
  while ((row) && (row[0]) && (row[1]) && (row[2]) && (row[3]) && (row[4]))
  {
    int id(atoi(row[0]));
    int min_lon(atoi(row[2]));
    int max_lon(atoi(row[4]));
    if (max_lon > lon)
    {
      if (min_lon < lon)
      {
	int min_lat(atoi(row[1]));
	int max_lat(atoi(row[3]));
	if ((min_lat < lat) && (max_lat < lat))
	  area_cands[id] = !area_cands[id];
	else if ((min_lat < lat) || (max_lat < lat))
	{
	  int rel_lat(((long long)(max_lat - min_lat))*(lon - min_lon)/(max_lon - min_lon) + min_lat);
	  if (rel_lat < lat)
	    area_cands[id] = !area_cands[id];
	  else if (rel_lat == lat)
	    //We are on a border segment.
	    area_definitives.insert(id);
	}
	else if ((min_lat == lat) && (max_lat == lat))
	  //We are on a horizontal border segment.
	  area_definitives.insert(id);
      }
      else if (min_lon == lon)
	//We are north of a node of the border.
	//We can safely count such a segment if and only if the node is
	//on its western end.
      {
	int min_lat(atoi(row[1]));
	if (min_lat < lat)
	  area_cands[id] = !area_cands[id];
	else if (min_lat == lat)
	  //We have hit a node of the border.
	  area_definitives.insert(id);
      }
    }
    else if (max_lon == lon)
    {
      int max_lat(atoi(row[3]));
      if (max_lat == lat)
	//We have hit a node of the border.
	area_definitives.insert(id);
      else if (min_lon == max_lon)
	//We are on a vertical border segment.
      {
	int min_lat(atoi(row[1]));
	if ((min_lat <= lat) && (lat <= max_lat))
	  area_definitives.insert(id);
      }
    }
    row = mysql_fetch_row(result);
  }
  
  while (mysql_fetch_row(result))
    ;
  mysql_free_result(result);
  for (set< int >::const_iterator it(area_definitives.begin());
       it != area_definitives.end(); ++it)
    result_set.insert(Area(*it));
  for (map< int, bool >::const_iterator it(area_cands.begin());
       it != area_cands.end(); ++it)
  {
    if (it->second)
      result_set.insert(Area(it->first));
  }
  return result_set;
}

set< int >& multiint_to_multiint_query
    (MYSQL* mysql, string prefix, string suffix, const set< int >& source, set< int >& result_set)
{
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      result_set.insert(atoi(row[0]));
      row = mysql_fetch_row(result);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

void multiint_to_null_query
    (MYSQL* mysql, string prefix, string suffix, const set< int >& source)
{
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix;
	
    mysql_query_null_wrapper(mysql, temp.str());
  }
  return;
}

set< Node >& multiint_to_multiNode_query
    (MYSQL* mysql, string prefix, string suffix, const set< int >& source, set< Node >& result_set)
{
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]) && (row[1]) && (row[2]))
    {
      result_set.insert(Node(atoi(row[0]), atoi(row[1]), atoi(row[2])));
      row = mysql_fetch_row(result);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

set< Way >& multiint_to_multiWay_query
    (MYSQL* mysql, string prefix, string suffix, const set< int >& source, set< Way >& result_set)
{
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      Way way(atoi(row[0]));
      way.members.reserve(10);
      while ((row) && (row[0]) && (way.id == atoi(row[0])))
      {
	if ((row[1]) && (row[2]))
	{
	  unsigned int count((unsigned int)atol(row[1]));
	  if (way.members.capacity() < count)
	    way.members.reserve(count+10);
	  if (way.members.size() < count)
	    way.members.resize(count);
	  way.members[count-1] = atoi(row[2]);
	}
	row = mysql_fetch_row(result);
      }
      result_set.insert(way);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

set< Relation >& multiint_to_multiRelation_query
    (MYSQL* mysql, 
     string prefix1, string suffix1, string prefix2, string suffix2, string prefix3, string suffix3,
     const set< int >& source, set< Relation >& result_set)
{
  map< int, set< pair< int, int > > > node_members;
  map< int, set< pair< int, int > > > way_members;
  map< int, set< pair< int, int > > > relation_members;
  
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix1;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix1;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      int id(atoi(row[0]));
      set< pair< int, int > > nodes;
      while ((row) && (row[0]) && (id == atoi(row[0])))
      {
	if (row[1])
	{
	  if (row[2])
	    nodes.insert
		(make_pair< int, int >(atoi(row[1]), atoi(row[2])));
	  else
	    nodes.insert
		(make_pair< int, int >(atoi(row[1]), 0));
	}
	row = mysql_fetch_row(result);
      }
      node_members[id] = nodes;
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix2;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix2;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      int id(atoi(row[0]));
      set< pair< int, int > > ways;
      while ((row) && (row[0]) && (id == atoi(row[0])))
      {
	if (row[1])
	{
	  if (row[2])
	    ways.insert
		(make_pair< int, int >(atoi(row[1]), atoi(row[2])));
	  else
	    ways.insert
		(make_pair< int, int >(atoi(row[1]), 0));
	}
	row = mysql_fetch_row(result);
      }
      way_members[id] = ways;
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  
  for (set< int >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix3;
    temp<<" ("<<*it;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<*it;
    temp<<") "<<suffix3;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      int id(atoi(row[0]));
      set< pair< int, int > > relations;
      while ((row) && (row[0]) && (id == atoi(row[0])))
      {
	if (row[1])
	{
	  if (row[2])
	    relations.insert
		(make_pair< int, int >(atoi(row[1]), atoi(row[2])));
	  else
	    relations.insert
		(make_pair< int, int >(atoi(row[1]), 0));
	}
	row = mysql_fetch_row(result);
      }
      relation_members[id] = relations;
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  
  for (set< int >::const_iterator it(source.begin()); it != source.end(); ++it)
  {
    Relation relation(*it);
    relation.node_members = node_members[*it];
    relation.way_members = way_members[*it];
    relation.relation_members = relation_members[*it];
    result_set.insert(relation);
  }
  
  return result_set;
}

set< int >& multiNode_to_multiint_query
    (MYSQL* mysql, string prefix, string suffix, const set< Node >& source, set< int >& result_set)
{
  for (set< Node >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<it->id;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<it->id;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      result_set.insert(atoi(row[0]));
      row = mysql_fetch_row(result);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

set< int >& multiWay_to_multiint_query
    (MYSQL* mysql, string prefix, string suffix, const set< Way >& source, set< int >& result_set)
{
  for (set< Way >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<it->id;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<it->id;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      result_set.insert(atoi(row[0]));
      row = mysql_fetch_row(result);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

set< int >& multiRelation_to_multiint_query
    (MYSQL* mysql, string prefix, string suffix, const set< Relation >& source, set< int >& result_set)
{
  for (set< Relation >::const_iterator it(source.begin()); it != source.end(); )
  {
    ostringstream temp;
    temp<<prefix;
    temp<<" ("<<it->id;
    unsigned int i(0);
    while (((++it) != source.end()) && (i++ < 10000))
      temp<<", "<<it->id;
    temp<<") "<<suffix;
	
    MYSQL_RES* result(mysql_query_use_wrapper(mysql, temp.str()));
    if (!result)
      return result_set;
	
    MYSQL_ROW row(mysql_fetch_row(result));
    while ((row) && (row[0]))
    {
      result_set.insert(atoi(row[0]));
      row = mysql_fetch_row(result);
    }
    
    while (mysql_fetch_row(result))
      ;
    mysql_free_result(result);
  }
  return result_set;
}

//-----------------------------------------------------------------------------

typedef short int int16;
typedef int int32;
typedef long long int64;

typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

//-----------------------------------------------------------------------------

set< Node >& multiint_to_multiNode_query(const set< int >& source, set< Node >& result_set)
{
  int nodes_dat_fd = open64(NODE_IDXA, O_RDONLY);
  if (nodes_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  
  set< int > blocks;
  int16 idx_buf(0);
  for (set< int >::const_iterator it(source.begin()); it != source.end(); ++it)
  {
    lseek64(nodes_dat_fd, ((*it)-1)*sizeof(int16), SEEK_SET);
    read(nodes_dat_fd, &idx_buf, sizeof(int16));
    blocks.insert(idx_buf);
  }
  
  close(nodes_dat_fd);
  
  int32* buf_count = (int32*) malloc(sizeof(int) + BLOCKSIZE*sizeof(Node));
  Node* nd_buf = (Node*) &buf_count[1];
  if (!buf_count)
  {
    runtime_error("Bad alloc in node query", cout);
    return result_set;
  }
  
  nodes_dat_fd = open64(NODE_DATA, O_RDONLY);
  if (nodes_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  
  for (set< int >::const_iterator it(blocks.begin()); it != blocks.end(); ++it)
  {
    lseek64(nodes_dat_fd, (int64)(*it)*(sizeof(int) + BLOCKSIZE*sizeof(Node)), SEEK_SET);
    read(nodes_dat_fd, buf_count, sizeof(int) + BLOCKSIZE*sizeof(Node));
    for (int32 i(0); i < buf_count[0]; ++i)
    {
      if (source.find(nd_buf[i].id) != source.end())
	result_set.insert(nd_buf[i]);
    }
  }
  
  close(nodes_dat_fd);
  
  free(buf_count);
  
  return result_set;
}

void multiRange_to_multiNode_query
    (const set< pair< int, int > >& in_inside, const set< pair< int, int > >& in_border,
     set< Node >& res_inside, set< Node >& res_border)
{
  vector< pair< int32, uint32 > > block_index;

  int nodes_idx_fd = open64(NODE_IDX, O_RDONLY);
  if (nodes_idx_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return;
  }
  
  int32* buf = (int32*) malloc(sizeof(int32)*2);
  while (read(nodes_idx_fd, buf, sizeof(int)*2))
    block_index.push_back(make_pair< int32, uint32 >(buf[0], buf[1]));
  close(nodes_idx_fd);
  
  int nodes_dat_fd = open64(NODE_DATA, O_RDONLY);
  if (nodes_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    free(buf);
    return;
  }
  
  int32* buf_count = (int32*) malloc(sizeof(int) + BLOCKSIZE*sizeof(Node));
  Node* nd_buf = (Node*) &buf_count[1];
  set< pair< int, int > >::const_iterator it_inside(in_inside.begin());
  set< pair< int, int > >::const_iterator it_border(in_border.begin());
  for (unsigned int i(1); i < block_index.size(); ++i)
  {
    bool block_inside((it_inside != in_inside.end()) &&
	(it_inside->first <= block_index[i].first));
    bool block_border((it_border != in_border.end()) &&
	(it_border->first <= block_index[i].first));
    if (block_inside || block_border)
    {
      lseek64(nodes_dat_fd,
	      (int64)(block_index[i-1].second)*(sizeof(int) + BLOCKSIZE*sizeof(Node)), SEEK_SET);
      read(nodes_dat_fd, buf_count, sizeof(int) + BLOCKSIZE*sizeof(Node));
    }
    if (block_inside)
    {
      for (int32 j(0); j < buf_count[0]; ++j)
      {
	int32 nd_idx(ll_idx(nd_buf[j].lat, nd_buf[j].lon));
	if (nd_idx >= it_inside->first)
	{
	  if (nd_idx <= it_inside->second)
	    res_inside.insert(nd_buf[j]);
	  else
	  {
	    while ((it_inside != in_inside.end()) && (it_inside->second < nd_idx))
	      ++it_inside;
	    if (it_inside == in_inside.end())
	      break;
	    if (nd_idx <= it_inside->second)
	      res_inside.insert(nd_buf[j]);
	  }
	}
      }
    }
    if (block_border)
    {
      for (int32 j(0); j < buf_count[0]; ++j)
      {
	int32 nd_idx(ll_idx(nd_buf[j].lat, nd_buf[j].lon));
	if (nd_idx >= it_border->first)
	{
	  if (nd_idx <= it_border->second)
	    res_border.insert(nd_buf[j]);
	  else
	  {
	    while ((it_border != in_border.end()) && (it_border->second < nd_idx))
	      ++it_border;
	    if (it_border == in_border.end())
	      break;
	    if (nd_idx <= it_border->second)
	      res_border.insert(nd_buf[j]);
	  }
	}
      }
    }
    while ((it_inside != in_inside.end()) && (it_inside->second < block_index[i].first))
      ++it_inside;
    while ((it_border != in_border.end()) && (it_border->second < block_index[i].first))
      ++it_border;
  }
  
  close(nodes_dat_fd);
  
  free(buf);
  free(buf_count);
}

int multiRange_to_count_query
    (const set< pair< int, int > >& in_inside, const set< pair< int, int > >& in_border)
{
  vector< pair< int32, uint32 > > block_index;

  int nodes_idx_fd = open64(NODE_IDX, O_RDONLY);
  if (nodes_idx_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return 0;
  }
  
  int32* buf = (int32*) malloc(sizeof(int32)*2);
  while (read(nodes_idx_fd, buf, sizeof(int)*2))
    block_index.push_back(make_pair< int32, uint32 >(buf[0], buf[1]));
  close(nodes_idx_fd);
  
  int count(0);
  set< pair< int, int > >::const_iterator it_inside(in_inside.begin());
  set< pair< int, int > >::const_iterator it_border(in_border.begin());
  for (unsigned int i(1); i < block_index.size(); ++i)
  {
    while ((it_inside != in_inside.end()) && (it_inside->second < block_index[i].first))
    {
      count += (long long)BLOCKSIZE*
	  (it_inside->second - it_inside->first + 1)/(block_index[i].first - block_index[i-1].first + 1);
      ++it_inside;
    }
    while ((it_border != in_border.end()) && (it_border->second < block_index[i].first))
    {
      count += (long long)BLOCKSIZE*
	  (it_border->second - it_border->first + 1)/(block_index[i].first - block_index[i-1].first + 1);
      ++it_border;
    }
  }
  
  free(buf);
  
  return count;
}

//-----------------------------------------------------------------------------

set< Way >& multiint_to_multiWay_query(const set< int >& source, set< Way >& result_set)
{
  int ways_dat_fd = open64(WAY_IDXA, O_RDONLY);
  if (ways_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  
  set< int > blocks;
  int16 idx_buf(0);
  for (set< int >::const_iterator it(source.begin()); it != source.end(); ++it)
  {
    lseek64(ways_dat_fd, ((*it)-1)*sizeof(int16), SEEK_SET);
    read(ways_dat_fd, &idx_buf, sizeof(int16));
    blocks.insert(idx_buf);
  }
  
  close(ways_dat_fd);
  
  uint32* way_buf = (uint32*) malloc(sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32));
  if (!way_buf)
  {
    runtime_error("Bad alloc in way query", cout);
    return result_set;
  }

  ways_dat_fd = open64(WAY_DATA, O_RDONLY);
  if (ways_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  
  for (set< int >::const_iterator it(blocks.begin()); it != blocks.end(); ++it)
  {
    lseek64(ways_dat_fd, (int64)(*it)*(sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32)), SEEK_SET);
    read(ways_dat_fd, way_buf, sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32));
    for (uint32 i(1); i < way_buf[0]; i += way_buf[i+2]+3)
    {
      if (source.find(way_buf[i]) != source.end())
      {
	Way way(way_buf[i]);
	for (uint32 j(0); j < way_buf[i+2]; ++j)
	  way.members.push_back(way_buf[j+i+3]);
	result_set.insert(way);
      }
    }
  }

  close(ways_dat_fd);
  
  free(way_buf);
  
  return result_set;
}

set< Way >& multiNode_to_multiWay_query(const set< Node >& source, set< Way >& result_set)
{
  int ways_dat_fd = open64(WAY_IDXSPAT, O_RDONLY);
  if (ways_dat_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  uint32 spat_idx_buf_size(lseek64(ways_dat_fd, 0, SEEK_END)/sizeof(pair< int32, uint16 >));
  pair< int32, uint16 >* spat_idx_buf = (pair< int32, uint16 >*)
      malloc(spat_idx_buf_size * sizeof(pair< int32, uint16 >));
  if (!spat_idx_buf)
  {
    runtime_error("Bad alloc in way query", cout);
    return result_set;
  }
  lseek64(ways_dat_fd, 0, SEEK_SET);
  read(ways_dat_fd, spat_idx_buf, spat_idx_buf_size * sizeof(pair< int32, uint16 >));
  close(ways_dat_fd);
  
  set< int > ll_idxs;
  for (set< Node >::const_iterator it(source.begin()); it != source.end(); ++it)
    ll_idxs.insert(ll_idx(it->lat, it->lon) & WAY_IDX_BITMASK);
  
  set< int > blocks;
  for (uint32 i(0); i < spat_idx_buf_size; ++i)
  {
    if (ll_idxs.find((spat_idx_buf[i].first)) != ll_idxs.end())
      blocks.insert(spat_idx_buf[i].second);
  }
  
  free(spat_idx_buf);
  
  uint32* way_buf = (uint32*) malloc(sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32));
  if (!way_buf)
  {
    runtime_error("Bad alloc in way query", cout);
    return result_set;
  }

  ways_dat_fd = open64(WAY_DATA, O_RDONLY);
  if (ways_dat_fd < 0)
  {
    free(way_buf);
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return result_set;
  }
  
  for (set< int >::const_iterator it(blocks.begin()); it != blocks.end(); ++it)
  {
    lseek64(ways_dat_fd, (int64)(*it)*(sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32)), SEEK_SET);
    read(ways_dat_fd, way_buf, sizeof(uint32) + WAY_BLOCKSIZE*sizeof(uint32));
    for (uint32 i(1); i < way_buf[0]; i += way_buf[i+2]+3)
    {
      for (uint32 j(0); j < way_buf[i+2]; ++j)
      {
	if (source.find(Node(way_buf[j+i+3], 200*10*1000*1000, 0)) != source.end())
	{
	  Way way(way_buf[i]);
	  for (j = 0; j < way_buf[i+2]; ++j)
	    way.members.push_back(way_buf[j+i+3]);
	  result_set.insert(way);
	}
      }
    }
  }

  close(ways_dat_fd);
  
  free(way_buf);
  
  return result_set;
}

//-----------------------------------------------------------------------------

void select_kv_to_idx
    (string key, string value, set< uint32 >& string_ids_global,
     set< uint32 >& string_ids_local, set< uint32 >& string_idxs_local)
{
  static vector< uint32 > spatial_boundaries;
  static vector< vector< pair< string, string > > > kv_to_id_idx;
  static vector< vector< uint16 > > kv_to_id_block_idx;
  
  if ((spatial_boundaries.empty()) || (kv_to_id_idx.empty()) || (kv_to_id_block_idx.empty()))
  {
    spatial_boundaries.clear();
    
    int string_idx_fd = open64(NODE_STRING_IDX, O_RDONLY);
    if (string_idx_fd < 0)
    {
      ostringstream temp;
      temp<<"open64: "<<errno;
      runtime_error(temp.str(), cout);
      return;
    }
  
    uint32* string_spat_idx_buf = (uint32*) malloc(NODE_TAG_SPATIAL_PARTS*sizeof(uint32));
    read(string_idx_fd, string_spat_idx_buf, NODE_TAG_SPATIAL_PARTS*sizeof(uint32));
    for (uint32 i(0); i < NODE_TAG_SPATIAL_PARTS; ++i)
      spatial_boundaries.push_back(string_spat_idx_buf[i]);
    free(string_spat_idx_buf);
    
    kv_to_id_idx.clear();
    kv_to_id_idx.resize(NODE_TAG_SPATIAL_PARTS+1);
    kv_to_id_block_idx.clear();
    kv_to_id_block_idx.resize(NODE_TAG_SPATIAL_PARTS+1);
    
    uint32* kv_to_id_idx_buf_1 = (uint32*) malloc(3*sizeof(uint16));
    char* kv_to_id_idx_buf_2 = (char*) malloc(2*64*1024);
    uint32 block_id(0);
    while (read(string_idx_fd, kv_to_id_idx_buf_1, 3*sizeof(uint16)))
    {
      read(string_idx_fd, kv_to_id_idx_buf_2,
	   ((uint16*)kv_to_id_idx_buf_1)[1] + ((uint16*)kv_to_id_idx_buf_1)[2]);
      kv_to_id_idx[((uint16*)kv_to_id_idx_buf_1)[0]].push_back(make_pair< string, string >
	  (string(kv_to_id_idx_buf_2, ((uint16*)kv_to_id_idx_buf_1)[1]),
	   string(&(kv_to_id_idx_buf_2[((uint16*)kv_to_id_idx_buf_1)[1]]),
		    ((uint16*)kv_to_id_idx_buf_1)[2])));
      kv_to_id_block_idx[((uint16*)kv_to_id_idx_buf_1)[0]].push_back(block_id++);
    }
    free(kv_to_id_idx_buf_2);
    free(kv_to_id_idx_buf_1);
  
    close(string_idx_fd);
  }
  
  set< uint16 > kv_to_idx_block_ids;
  for (uint32 i(0); i < NODE_TAG_SPATIAL_PARTS+1; ++i)
  {
    uint32 j(1);
    if (value == "")
    {
      while ((j < kv_to_id_idx[i].size()) && (kv_to_id_idx[i][j].first < key))
	++j;
      kv_to_idx_block_ids.insert(kv_to_id_block_idx[i][j-1]);
      while ((j < kv_to_id_idx[i].size()) && (kv_to_id_idx[i][j].first <= key))
      {
	kv_to_idx_block_ids.insert(kv_to_id_block_idx[i][j]);
	++j;
      }
    }
    else
    {
      while ((j < kv_to_id_idx[i].size()) && ((kv_to_id_idx[i][j].first < key) ||
	      ((kv_to_id_idx[i][j].first == key) && (kv_to_id_idx[i][j].second < value))))
	++j;
      kv_to_idx_block_ids.insert(kv_to_id_block_idx[i][j-1]);
      while ((j < kv_to_id_idx[i].size()) && ((kv_to_id_idx[i][j].first < key) ||
	      ((kv_to_id_idx[i][j].first == key) && (kv_to_id_idx[i][j].second <= value))))
      {
	kv_to_idx_block_ids.insert(kv_to_id_block_idx[i][j]);
	++j;
      }
    }
  }

  int string_fd = open64(NODE_STRING_DATA, O_RDONLY);
  if (string_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return;
  }
  
  char* string_idxs_buf = (char*) malloc(NODE_STRING_BLOCK_SIZE);
  if (value == "")
  {
    for (set< uint16 >::const_iterator it(kv_to_idx_block_ids.begin());
	 it != kv_to_idx_block_ids.end(); ++it)
    {
      lseek64(string_fd, ((uint64)(*it))*NODE_STRING_BLOCK_SIZE, SEEK_SET);
      read(string_fd, string_idxs_buf, NODE_STRING_BLOCK_SIZE);
      uint32 pos(sizeof(uint32));
      while (pos < *((uint32*)string_idxs_buf + sizeof(uint32)))
      {
	pos += 2*sizeof(uint32);
	uint16& key_len(*((uint16*)&(string_idxs_buf[pos])));
	pos += sizeof(uint16);
	uint16& value_len(*((uint16*)&(string_idxs_buf[pos])));
	pos += sizeof(uint16);
	if (!(strncmp(key.c_str(), &(string_idxs_buf[pos]), key_len)))
	{
	  if (*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - sizeof(uint32)]))
			== 0xffffff00)
	    string_ids_global.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - 2*sizeof(uint32)])));
	  else
	  {
	    string_idxs_local.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - sizeof(uint32)])));
	    string_ids_local.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - 2*sizeof(uint32)])));
	  }
	}
	pos += key_len + value_len;
      }
    }
  }
  else
  {
    for (set< uint16 >::const_iterator it(kv_to_idx_block_ids.begin());
	 it != kv_to_idx_block_ids.end(); ++it)
    {
      lseek64(string_fd, ((uint64)(*it))*NODE_STRING_BLOCK_SIZE, SEEK_SET);
      read(string_fd, string_idxs_buf, NODE_STRING_BLOCK_SIZE);
      uint32 pos(sizeof(uint32));
      while (pos < *((uint32*)string_idxs_buf) + sizeof(uint32))
      {
	pos += 2*sizeof(uint32);
	uint16& key_len(*((uint16*)&(string_idxs_buf[pos])));
	pos += sizeof(uint16);
	uint16& value_len(*((uint16*)&(string_idxs_buf[pos])));
	pos += sizeof(uint16);
	if ((!(strncmp(key.c_str(), &(string_idxs_buf[pos]), key_len))) &&
		      (!(strncmp(value.c_str(), &(string_idxs_buf[pos + key_len]), value_len))))
	{
	  if (*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - sizeof(uint32)]))
			== 0xffffff00)
	    string_ids_global.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - 2*sizeof(uint32)])));
	  else
	  {
	    string_idxs_local.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - sizeof(uint32)])));
	    string_ids_local.insert
		(*((uint32*)&(string_idxs_buf[pos - 2*sizeof(uint16) - 2*sizeof(uint32)])));
	  }
	}
	pos += key_len + value_len;
      }
    }
  }
  free(string_idxs_buf);
  
  close(string_fd);
}

void select_idx_to_node_local
    (const set< uint32 >& source, const set< uint32 >& src_idxs, set< int >& result)
{
  const char* IDX_FILE = FOO;
  const char* DATA_FILE = FOO;
  
  static vector< uint32 > idx_boundaries;
  static vector< uint16 > block_idx;
  
  if ((idx_boundaries.empty()) || (block_idx.empty()))
  {
    idx_boundaries.clear();
    block_idx.clear();
    
    int idx_fd = open64(IDX_FILE, O_RDONLY);
    if (idx_fd < 0)
    {
      ostringstream temp;
      temp<<"open64: "<<errno;
      runtime_error(temp.str(), cout);
      return;
    }
  
    uint32 idx_file_size(lseek64(idx_fd, 0, SEEK_END));
    char* idx_buf = (char*) malloc(idx_file_size);
    lseek64(idx_fd, 0, SEEK_SET);
    read(idx_fd, idx_buf, idx_file_size);
    uint32 i(0);
    while (i < idx_file_size / (sizeof(uint32) + sizeof(uint16)))
    {
      idx_boundaries.push_back(*((uint32*)&(idx_buf[i])));
      i += sizeof(uint32);
      block_idx.push_back(*(uint16*)&(idx_buf[i]));
      i += sizeof(uint16);
    }
    free(idx_buf);
  
    close(idx_fd);
  }
  
  set< uint16 > block_ids;
  uint32 i(1);
  set< uint32 >::const_iterator it(src_idxs.begin());
  while (it != src_idxs.end())
  {
    while ((i < idx_boundaries.size()) && (idx_boundaries[i] < *it))
      ++i;
    block_ids.insert(block_idx[i-1]);
    while ((i < idx_boundaries.size()) && (idx_boundaries[i] <= *it))
    {
      block_ids.insert(block_idx[i]);
      ++i;
    }
    ++it;
  }

  int data_fd = open64(DATA_FILE, O_RDONLY);
  if (data_fd < 0)
  {
    ostringstream temp;
    temp<<"open64: "<<errno;
    runtime_error(temp.str(), cout);
    return;
  }
  
  char* data_buf = (char*) malloc(BLOCKSIZE);
  for (set< uint16 >::const_iterator it(block_ids.begin());
       it != block_ids.end(); ++it)
  {
    lseek64(data_fd, ((uint64)(*it))*BLOCKSIZE, SEEK_SET);
    read(data_fd, data_buf, BLOCKSIZE);
    uint32 pos(sizeof(uint32));
    while (pos < *((uint32*)data_buf) + sizeof(uint32))
    {
      if (source.find(*((uint32*)data_buf[pos])) != source.end())
      {
	pos += sizeof(uint32);
	for (uint32 j(0); j < *((uint32*)data_buf[pos]); ++j)
	  result.insert(*((uint32*)data_buf[pos + sizeof(uint16) + j*sizeof(uint32)]));
	pos -= sizeof(uint32);
      }
      pos += sizeof(uint32);
      pos += *((uint32*)data_buf[pos])*sizeof(uint32) + sizeof(uint16);
    }
  }
  free(data_buf);
  
  close(data_fd);
}

set< int >& kv_to_multiint_query(string key, string value, set< int >& result_set)
{
  set< uint32 > string_ids_global;
  set< uint32 > string_ids_local;
  set< uint32 > string_idxs_local;
  select_kv_to_idx(key, value, string_ids_global, string_ids_local, string_idxs_local);
  
  //TEMP
  cout<<"global: ";
  for (set< uint32 >::const_iterator it(string_ids_global.begin()); it != string_ids_global.end(); ++it)
    cout<<*it<<' ';
  cout<<"\nlocal: ";
  set< uint32 >::const_iterator it2(string_idxs_local.begin());
  for (set< uint32 >::const_iterator it(string_ids_local.begin()); it != string_ids_local.end(); ++it)
    cout<<*it<<' '<<*(it++)<<"   ";
  cout<<'\n';
  
  return result_set;
}