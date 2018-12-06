/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file hash_map.c
* @date 9 Nov 2018
* @brief This header implements a simple hash map data structure
* @author Andrea Piccione
*
* This a simple hash map implementation, currently used in the abm layer.
* It's based on a open addressing design:
* it handles collisions through linear probing,
* entries are reordered on insertion through robin hood hashing.
*
* TODO : some code cleanup
*/


#include <datatypes/hash_map.h>
#include <mm/dymelor.h>
#include <memory.h>
#include <limits.h>

// must be a power of two
#define HM_INITIAL_CAPACITY 8
#define DIB(curr_i, hash, capacity_mo) (((curr_i) - (hash)) & (capacity_mo))
#define SWAP_VALUES(a, b) do{__typeof(a) _tmp = (a); (a) = (b); (b) = _tmp;}while(0)

// Adapted from http://xorshift.di.unimi.it/splitmix64.c PRNG,
// written by Sebastiano Vigna (vigna@acm.org)
// TODO benchmark further and select a possibly better hash function
static hash_t _get_hash(unsigned long long key){
	uint64_t z = key + 0x9e3779b97f4a7c15;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return (hash_t)((z ^ (z >> 31)) >> 32);
}

void hash_map_init(rootsim_hash_map_t *hmap){
	// this is the effective capacity_minus_one
	// this trick saves us some subtractions when we
	// use the capacity as a bitmask to
	// select the relevant hash bits for table indexing
	hmap->capacity_mo = HM_INITIAL_CAPACITY - 1;
	hmap->count = 0;
	hmap->nodes = rsalloc(sizeof(struct _hash_map_node_t) * HM_INITIAL_CAPACITY);
	memset(hmap->nodes, 0, sizeof(struct _hash_map_node_t) * HM_INITIAL_CAPACITY);
}

void hash_map_fini(rootsim_hash_map_t *hmap){
	__wrap_free(hmap->nodes);
}

static void _hash_map_insert_hashed(rootsim_hash_map_t *hmap, hash_map_pair_t *pair, hash_t hash){
	struct _hash_map_node_t *nodes = hmap->nodes;
	map_size_t capacity_mo = hmap->capacity_mo;

	hash_map_pair_t *cur_pair = pair;
	hash_t cur_hash = hash;
	// since capacity_mo is 2^n - 1 for some n
	// this effectively is a modulo 2^n
	map_size_t i = cur_hash & capacity_mo;
	// dib stays for distance from initial bucket
	map_size_t dib = 0;

	// linear probing with robin hood hashing
	// https://cs.uwaterloo.ca/research/tr/1986/CS-86-14.pdf by Pedro Celis
	while(1){
		if(nodes[i].pair == NULL){
			// found an empty cell, put the pair here and we're done
			nodes[i].pair = cur_pair;
			nodes[i].hash = cur_hash;
			return;
		} else if(dib > DIB(i, nodes[i].hash, capacity_mo)){
			// found a "richer" cell, swap the pairs and continue looking for a hole
			dib = DIB(i, nodes[i].hash, capacity_mo);
			SWAP_VALUES(cur_pair, nodes[i].pair);
			SWAP_VALUES(cur_hash, nodes[i].hash);
		}
		++i;
		// modulo capacity
		i &= capacity_mo;
		++dib;
	}
}

static void _hash_map_realloc_rehash(rootsim_hash_map_t *hmap){
	// helper pointers to iterate over the old array
	struct _hash_map_node_t *rmv = hmap->nodes;
	// instantiates new array
	hmap->nodes = rsalloc(sizeof(struct _hash_map_node_t) * (hmap->capacity_mo + 1));
	memset(hmap->nodes, 0, sizeof(struct _hash_map_node_t) * (hmap->capacity_mo + 1));
	// rehash the old array elements
	map_size_t i = hmap->count, j = 0;
	while(i--){
		while(rmv[j].pair == NULL)
			++j;
		// TODO: implement more efficient rehashing (in place rehashing)
		_hash_map_insert_hashed(hmap, rmv[j].pair, rmv[j].hash);
		++j;
	}
	// free the old array
	rsfree(rmv);
}

static void _hash_map_expand(rootsim_hash_map_t *hmap){
	// check if threshold has been reached
	if((double)hmap->capacity_mo * MAX_LOAD_FACTOR >= hmap->count)
		return;
	// increase map capacity (remember capacity_minus_one)
	hmap->capacity_mo = 2 * hmap->capacity_mo + 1;

	_hash_map_realloc_rehash(hmap);
}

static void _hash_map_shrink(rootsim_hash_map_t *hmap){
	// check if threshold has been reached
	if((double)hmap->capacity_mo * MIN_LOAD_FACTOR <= hmap->count ||
			hmap->capacity_mo <= HM_INITIAL_CAPACITY)
		return;
	// decrease map capacity (remember capacity_minus_one)
	hmap->capacity_mo /= 2;

	_hash_map_realloc_rehash(hmap);
}

void hash_map_add(rootsim_hash_map_t *hmap, hash_map_pair_t *pair){
	// expand if needed
	_hash_map_expand(hmap);
	// insert the element
	_hash_map_insert_hashed(hmap, pair, _get_hash(pair->key));
	hmap->count++;
}

static map_size_t _hash_map_index_lookup(rootsim_hash_map_t *hmap, unsigned long long key){
	struct _hash_map_node_t *nodes = hmap->nodes;
	map_size_t capacity_mo = hmap->capacity_mo;

	hash_t cur_hash = _get_hash(key);
	map_size_t i = cur_hash & capacity_mo;
	map_size_t dib = 0;

	do{
		if(nodes[i].pair == NULL){
			// we found a hole where we expected something, the pair hasn't been found
			return UINT_MAX;
		//  the more expensive comparison with the key is done only if necessary
		} else if(nodes[i].hash == cur_hash && nodes[i].pair->key == key)
			// we found a pair with coinciding keys, return the index
			return i;
		++i;
		i &= capacity_mo;
		++dib;
	}while(dib <= DIB(i, nodes[i].hash, capacity_mo));
	// we found a node- with lower DIB than expected: the wanted key isn't here
	// (else it would have finished here during its insertion).
	return UINT_MAX;
}

hash_map_pair_t* hash_map_lookup(rootsim_hash_map_t *hmap, unsigned long long key){
	// find the index of the wanted key
	map_size_t i = _hash_map_index_lookup(hmap, key);
	// return the pair if successful
	return i == UINT_MAX ? NULL : hmap->nodes[i].pair;
}

hash_map_pair_t* hash_map_remove(rootsim_hash_map_t *hmap, unsigned long long key){
	// find the index of the wanted key
	map_size_t i = _hash_map_index_lookup(hmap, key);
	// if unsuccessful we're done, nothing to remove here!
	if(i == UINT_MAX) return NULL;

	// save the pointer to return
	hash_map_pair_t *ret = hmap->nodes[i].pair;

	struct _hash_map_node_t *nodes = hmap->nodes;
	map_size_t capacity_mo = hmap->capacity_mo;
	map_size_t j = i;
	// backward shift to restore the table state as if the insertion never happened:
	// http://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion by Emmanuel Goossaert
	do{ // the first iteration is necessary since the removed element is always overwritten somehow
		++j;
		j &= capacity_mo;
	}while(nodes[j].pair != NULL && DIB(j, nodes[j].hash, capacity_mo) != 0);
	// we finally found out the end of the displaced sequence of nodes,
	// since we either found an empty slot or we found a node residing in his correct position

	--j; // we now point to the last element to move
	j &= capacity_mo;

	if(j >= i){
		// we simply move the nodes one slot back
		memmove(&nodes[i], &nodes[i + 1], (j - i) * sizeof(struct _hash_map_node_t));
	}else{
		// we wrapped around the table: move the first part back
		memmove(&nodes[i], &nodes[i + 1], (capacity_mo - i) * sizeof(struct _hash_map_node_t));
		// move the first node to the last slot in the table
		memcpy(&nodes[capacity_mo], &nodes[0], sizeof(struct _hash_map_node_t));
		// move the remaining stuff at the table beginning
		memmove(&nodes[0], &nodes[1], j * sizeof(struct _hash_map_node_t));
	}

	// we clear the last moved slot (if we didn't move anything this clears the removed entry)
	nodes[j].pair = NULL;

	// shrink the table if necessary
	hmap->count--;
	_hash_map_shrink(hmap);
	return ret;
}

hash_map_pair_t* hash_map_iter(rootsim_hash_map_t *hmap, map_size_t *closure){
	// simply walk the nodes array skipping empty slots
	while(1){
		if(*closure > hmap->capacity_mo){
			// finished our iteration; reset the closure and return NULL
			(*closure) = 0;
			return NULL;
		}
		if(hmap->nodes[*closure].pair == NULL)
			// skip this empty slot
			(*closure)++;
		else
			// return the pair and increment the closure
			return hmap->nodes[(*closure)++].pair;
	}
}

inline map_size_t hash_map_count(rootsim_hash_map_t *hmap){
	return hmap->count;
}
