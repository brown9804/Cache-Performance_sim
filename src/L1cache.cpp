/*
 *  Cache simulation project
 *  Class UCR IE-521
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <math.h>
#include <debug_utilities.h>
#include <L1cache.h>

#define KB 1024
#define ADDRSIZE 32
using namespace std;

// function.
// Checking the parameters status, on boundary etc
int params_check(int idx, int tag, int associativity){
  int ok_param = OK;
  int assocPower2 = associativity;
  if (idx >= 0 && tag >= 0 && associativity > 0) {
    //we need to checks if associativity is a power of 2, based on the spacial principle
    while(assocPower2%2==0 && assocPower2 > 1){ // across all values power of two.
      assocPower2 /= 2;
    }
    if (associativity%2!=0){
      ok_param = PARAM;
    }
    if (assocPower2==1){
      ok_param = OK;
    }else {
      ok_param = PARAM;
    }
  } else {
    ok_param = PARAM;
  }
  return ok_param;
}
////////////////////////////////////////

int field_size_get(struct cache_pararms cache_params,
                   struct cache_field_size *field_size)
{
   (*field_size).idx = log2((cache_params.size * KB) / (cache_params.block_size * cache_params.asociativity));
   (*field_size).offset = log2(cache_params.block_size);
   (*field_size).tag = ADDRSIZE - (*field_size).offset - (*field_size).idx;

   if ((*field_size).offset + (*field_size).idx + (*field_size).tag == ADDRSIZE)
   {
      return OK; 
   }
   else // err on field size 
   {
      return ERROR;
   }
}

void address_tag_idx_get(long address,
                         struct cache_field_size field_size,
                         int *idx,
                         int *tag)
{
   long indexMASK = (1 << field_size.idx) - 1;
   long tagMASK = (1 << field_size.tag) - 1;
   *idx = (address >> field_size.offset) & indexMASK;
   *tag = (address >> (field_size.offset + field_size.idx)) & tagMASK;
}

// ------------- BEGIN - BROWN, BELINDA ----------------
// @brown9804 Belinda Brown
// timna.brown@ucr.ac.cr
// November, 2020

// SRRIP Replacement Policy
// Stactic RRIP - Re-reference Interval Prediction
// M-bit Re-reference per cache block
// RRPV = how soon the block will be Rr
// ~ML by access pattern
// 1. RRPV == 0 ... Rr in near immediate future [~now]
// 2. RRPV = 2^M -1 ... RR in a distant future  [distant]
// Then, RRIP add new blocks with a long Rr interval
// from distant future to higher RRPV
// When M == 1 -> NMRU replacement policy
// Avoid corrected date by distant Rr blocks

// HP = Hit Priority (RRPV = 0)
// FP = Frequency Priority (RRPV = RRPV - 1)

// %%%%%%%%%%%%%%%%%%%%%%%%
// t=0 ... 2^2-1 = 3
// find-Victim-RRPIP(): on miss
// access-Block(): on hit  rrpv = 0 .. block(HP)
// insert-Block(): clear if valid and set rrpv as 2^2-2=2

//*** Cache hit:
// 1. RRPV of block to 0

//*** Cache miss:
// 1. Search from 2^M - 1 from left
// 2. if 2^M-1 is forund -> go to step(v)
// 3. Then increment all RRPVs
// 4. Again 1.
// 5. Replace block and put RRPV = 2^M-2
int srrip_replacement_policy (int idx,
                             int tag,
                             int associativity,
                             bool loadstore,
                             entry* cache_blocks,
                             operation_result* result,
                             bool debug)
{
   ///////////////////////////////////////
   // Variables 
   ///////////////////////////////////////
   int M;
   int space_empty;
   int distant_Block;
   int distant = pow(2, M) - 1;   // distant victim
   int long_rrpv = pow(2, M) - 2; // for new block with a long Rr interval
   ///// FLAGS
   bool hit_found_YorN = false;
   bool empty_found_YorN = false;
   bool higher_RRPV = false;

   ///////////////////////////////////////
   // Validate initial data
   ///////////////////////////////////////
      // idx, tag, associativity < 0
   if (idx < 0 || tag < 0 || associativity < 0)
   {
      return PARAM;
   }
   else {
	   // associativity <= 2
	   if (associativity <= 2)
	   {
	      M = 1;
	   }
	   // if not
	   else
	   {
	      M = 2;
	   }

   ///////////////////////////////////////
   // Stactic RRIP - Re-reference Interval Prediction
   ///////////////////////////////////////
   //*********************
   // Empty | Hit | Rr distant
   //*********************
   for (int i = 0; i < associativity; i++)
   {
   //*********************
   // If Hit
   //*********************

      if (cache_blocks[i].valid && tag == cache_blocks[i].tag)
      { 
      // When hit 
         hit_found_YorN = true;
         cache_blocks[i].rp_value = 0;
         if (loadstore == true)
         { 
         // Write 
            cache_blocks[i].dirty = 1;
            (*result).dirty_eviction = false;
            (*result).miss_hit = HIT_STORE;
         }
         else
         { // Read
            (*result).dirty_eviction = false;
            (*result).miss_hit = HIT_LOAD;
         }
         return OK;
      } // close cache_blocks[i].valid && tag == cache_blocks[i].tag
	   
      // Save empty ->
      else if (!cache_blocks[i].valid && !empty_found_YorN)
      {
         empty_found_YorN = true;
         space_empty = i;
      }
      // Save distant -> block 
      if ((cache_blocks[i].rp_value == distant) && (!higher_RRPV))
      {
         higher_RRPV = true;
         distant_Block = i;
      }
  } // end for (int i = 0; i < associativity; i++)

   ///////////////////////////////////////
   // Stactic RRIP - Re-reference Interval Prediction
   ///////////////////////////////////////
   //*********************
   // EMPTY = SPACE and Misses
   //*********************
   if ((empty_found_YorN == true) && (hit_found_YorN == false))
   {
      cache_blocks[space_empty].valid = 1;
      cache_blocks[space_empty].tag = tag;
      cache_blocks[space_empty].rp_value = long_rrpv;
      if (loadstore)
      { 
      // Write
         (*result).dirty_eviction = false;
         cache_blocks[space_empty].dirty = 1;
         (*result).miss_hit = MISS_STORE;
      }
      else
      { 
      // Read
         (*result).dirty_eviction = false;
         cache_blocks[space_empty].dirty = 0;
         (*result).miss_hit = MISS_LOAD;
      }
   } // end if ((empty_found_YorN == true) && (hit_found_YorN == false))

   ///////////////////////////////////////
   // Stactic RRIP - Re-reference Interval Prediction
   ///////////////////////////////////////
   //*********************
   // FULL or MISS
   //*********************
   else if ((empty_found_YorN == false) && (hit_found_YorN == false))
   {  
   ImmediateFuture:
      if (higher_RRPV == true)
      {
         if (cache_blocks[distant_Block].dirty)
         { // write-back
            (*result).dirty_eviction = true;
         }
         else { 
         // vic block
            cache_blocks[distant_Block].valid = 1;
            cache_blocks[distant_Block].tag = tag;
            cache_blocks[distant_Block].rp_value = long_rrpv;
	// result 
            (*result).dirty_eviction = false;
            (*result).evicted_address = cache_blocks[distant_Block].tag;

         } // end !cache_blocks[distant_Block].dirty 
	      
         if (loadstore == true)
         { 
         // Write
            (*result).miss_hit = MISS_STORE;
            cache_blocks[distant_Block].dirty = 1;
         } // end write 
         else
         { 
         // Read
            (*result).miss_hit = MISS_LOAD;
            cache_blocks[distant_Block].dirty = 0;
         } // end read 
	      
      } //  end if (higher_RRPV == true)
   //*********************
   // ELSE 
   //*********************
      else // begin higher_RRPV == false
      {
         for (int i = 0; i < associativity; i= i + 1)
         {
            cache_blocks[i].rp_value++;
            if ((cache_blocks[i].rp_value == distant) && (!higher_RRPV) )
            {
               higher_RRPV = true;
               distant_Block = i;
            }
         } // end  for (int i = 0; i < associativity; i= i + 1)
         goto ImmediateFuture ;
      } // end  else // begin higher_RRPV == false
   } // end if ((empty_found_YorN == false) && (hit_found_YorN == false))
   return OK;
   } // end big one else ---- else of (idx < 0 || tag < 0 || associativity < 0)
} // end policy srrip

////////////////////////////////////////////////////////////////////	   	   
// https://www.tutorialspoint.com/cplusplus/cpp_goto_statement.htm
// #include <iostream>
// using namespace std;
 
// int main () {
//    // Local variable declaration:
//    int a = 10;

//    // do loop execution
//    LOOP:do {
//       if( a == 15) {
//          // skip the iteration.
//          a = a + 1;
//          goto LOOP;
//       }
//       cout << "value of a: " << a << endl;
//       a = a + 1;
//    } 
//    while( a < 20 );
 
//    return 0;
// }	
////////////////////////////////////////////////////////////////////	 

// ------------- END - BROWN, BELINDA ----------------

// ------------- BEGIN - ESQUIVEL, BRANDON ----------------

  /*LRU Policy:
The LRU algorithm (less recently used for its acronym in English), tries to reduce the possibility
of discarding blocks that are going to be used again in the near future. For this, the information
regarding the use of the blocks is stored so that in a replacement the block that has not been used
for a longer time is discarded. LRU is based on the principle of temporary locality, according to which
if a block has been used recently it is likely to be used again in the near future, hence the best
candidate to replace is the block that has not been used for more. weather. The implementation of this
algorithm requires keeping a record of the use of each block so that updating the use of one block
implies updating that of all the others, hence this implementation tends to be very expensive for
highly associativity caches. This algorithm works as a queue-like data structure, hence it is referred
to as the LRU queue. The beginning of the queue (head), and therefore the candidate to leave it first,
is the block that has been referenced least recently (LRU block). Likewise, the end of the queue (queue)
and therefore the candidate to stay longer in it, is the block that has been most recently referenced
(MRU block, from the most recently used English). Since this algorithm is based on the principle of
temporal locality, it performs well against loads with high temporal locality in the data, but in
the case of loads where new references occur in the distant future, LRU performs poorly. As an example of the latter
type of loads there is the case in which the working set is greater than the capacity of the cache or
when a group of data without any type of reuse supposes the replacement of the data of the working set.*/
int lru_replacement_policy (int idx,
                             int tag,
                             int associativity,
                             bool loadstore,
                             entry* cache_blocks,
                             operation_result* result,
                             bool debug)
{
  // Control Flags
  bool cache_full = true;          // for eviction
  bool Hit_flag = false;           // one flag 1 for hit, 0 for miss
  
  // Ints
  int MRU_value = associativity - 1;      // Most recently used value
  int LRU_value = 0;                      // Lest recently used value.
  int Victim_value = -1;                        // Victim  value
  int hit_way = -1;                       // number of the hit way analyzed
  int free_way = -1;                       // number of the free way analyzed
  
  // getting parameters checked
  int param_check = params_check(idx, tag, associativity);      // returns Ok or PARAM
  if (param_check!=OK){
    return PARAM;
  }
  // checking all the ways
  for (int way = 0; way < associativity; way++){

    // LETS CHECK IF Hit
    if (cache_blocks[way].valid && cache_blocks[way].tag == tag){     // There will only be hit if the block in the current way is valid and the tag is the one indicated
      Hit_flag = true;
      hit_way = way;
    }
    //Space check, tracking of free ways and full cache
    if (cache_blocks[way].valid == false) {
      cache_full = false;
      free_way = way;
    }
  }
  // check hit status
  if (Hit_flag){
    //if was a Store
    if (loadstore){
      result->miss_hit = HIT_STORE;  
    //if was a Load
    }else{
      result->miss_hit = HIT_LOAD;
    }
    result->evicted_address = 0;      // not an address
    result->dirty_eviction = false; // not evicted

    // checking the replacement policy value
    for (int i = 0; i < associativity; i++){                // across all ways
      if (cache_blocks[i].rp_value>cache_blocks[hit_way].rp_value){   // limits of desicion
        if (cache_blocks[i].rp_value!=LRU_value){             
          cache_blocks[i].rp_value = cache_blocks[i].rp_value - 1;  // LRU value substraccion, policy.
        }
      }
    }
    cache_blocks[hit_way].rp_value = MRU_value; // now if the hit gets the maximun replacement value
    if(loadstore){     // Under a operation whit hit
      cache_blocks[hit_way].dirty = true;
    }
  }else{ // if we have a Miss
    if (loadstore){    // operation Store
      result->miss_hit = MISS_STORE;   // a store with a miss

    }else{     // If we have a Load
      result->miss_hit = MISS_LOAD;     // a Load with a miss
    }
    if (cache_full == false){     // now case of Free Space, if is not full
      result->dirty_eviction = false;     // not necessary an eviction
      result->evicted_address = 0;        // setting address to zero
      // checking all ways.
      for (int i = 0; i < associativity; i++){
        if (cache_blocks[i].valid){       // the block is valid
          if (cache_blocks[i].rp_value != LRU_value){    // the same logic, to update de Replacement policy value
            cache_blocks[i].rp_value = cache_blocks[i].rp_value - 1;  // substraction on the RP value
            }
        }
      }
      if (loadstore){    // lets check the dirty bit
        cache_blocks[free_way].dirty = true;   // if it was a operation over the block, set the dirty bit
      }else{
        cache_blocks[free_way].dirty = false;     // clean by default
      }
      cache_blocks[free_way].rp_value = MRU_value;  // now if the hit gets the maximun replacement value, the most recently used value.
      cache_blocks[free_way].tag = tag;             // update tag
      cache_blocks[free_way].valid = true;          // update valid
    }else{          // Now we have the last case: if cache is full
      // again checking all ways
      for (int i = 0; i < associativity; i++){
        if (cache_blocks[i].rp_value == LRU_value){   // checking for the victim 
          Victim_value = i;     //now set the Victim value
        }else{
          cache_blocks[i].rp_value = cache_blocks[i].rp_value -1;  // substraction on the RP value
        }
      }
      result->evicted_address = cache_blocks[Victim_value].tag;       // saving to result the address of the victim block with tag
      if (cache_blocks[Victim_value].dirty){
        result->dirty_eviction = true;        // update dirty eviction 
      }else{
        result->dirty_eviction = false;
      }
  
      if (loadstore){     //Now is necessary to check the dirty bits of the victims in order to know if store/load is needed
        cache_blocks[Victim_value].dirty = true;
      }else{
        cache_blocks[Victim_value].dirty = false;
      }
      cache_blocks[Victim_value].rp_value = MRU_value;    // again back and gets the maximun replacement value, the most recently used value.
      cache_blocks[Victim_value].valid = true;
      cache_blocks[Victim_value].tag = tag;


    }
  }
  return OK;        // param type return 
}
  
  
  // ------------- END - ESQUIVEL, BRANDON ----------------
  





// ------------- BEGIN - RAMIREZ, JONATHAN ----------------
/* @Jonsofmath Jonathan Ramírez Hernández.
   jonathan.ramirez15@ucr.ac.cr
   November, 2020

   NRU replacement policy. 
   Algorithm:
   In a cache HIT:
   	1. Set NRU-bit of block to '0'
   In a cache MISS:
   	1. Search for the first '1' from left.
	2. If '1' found go to step (5). 
	3. Set all NRU-bits to '1'
	4. Go to step (1)
	5. Replace block and set NRU-bit to '1'. */

int nru_replacement_policy(int idx,
                           int tag,
                           int associativity,
                           bool loadstore,
                           entry* cache_blocks,
                           operation_result* operation_result,
                           bool debug)
{
	bool miss_find = false;
	HIT:
		bool tag_found = false;
		for(int i=0; i < associativity; i++)
		{
			if(cache_blocks[idx*associativity+i].tag == tag&&tag_found == 0&&cache_blocks[idx*associativity+i].valid==1) /*HIT*/
			{
				if(loadstore == 0) /*Load*/
				{	
					(*operation_result).miss_hit = HIT_LOAD;
				}
				else /*Store*/
				{
					(*operation_result).miss_hit = HIT_STORE;
				}
				cache_blocks[idx*associativity+i].rp_value = 0;
				cache_blocks[idx*associativity+i].dirty = 0; /* No write */
				operation_result->dirty_eviction = 0; /* NO eviction, there's a hit */
				operation_result->evicted_address = 0; /* NO address */
				miss_find = false;
				tag_found = true;
			}	
			if(tag_found == 0)
			{
				if(i == associativity-1)	/*Set is done?*/ /*MISS*/
				{
					miss_find = true;
				}
			}
		}
	
	VICTIM_SEARCH:
		if(miss_find == true)
		{
			if(loadstore == 0) /*Load*/
			{
				(*operation_result).miss_hit = MISS_LOAD;
			}
			else /*Store*/
			{
				(*operation_result).miss_hit = MISS_STORE;
			}
				
			/* NRU EMPTY WAY/VICTIM SEARCH */
			bool victim_found = false;
			bool one_found = false;
			bool empty_found = false;
			int way;
			
			/* EMPTY WAY */
			for(int j=0; j < associativity; j++)
			{
				if(cache_blocks[idx*associativity+j].valid == 0&&empty_found==0)
				{
					way = j;
					empty_found = true;
				}
			}		
			
			/* VICTIM SEARCH */
			if(empty_found == 0)
			{
				while(victim_found==false)
				{
					for(int j=0; j < associativity; j++)
					{
						if(cache_blocks[idx*associativity+j].rp_value == 1&&one_found == 0) /*Victim found*/
						{
							victim_found = true;
							one_found = true;
							way = j;
						}
						else
						{
							if(j == associativity-1) /*Set is done?*/ /*There's no 1 found*/
							{
								for(int k=0; k < associativity; k++)
								{
									cache_blocks[idx*associativity+k].rp_value = 1;
								}
							}
						}
					}
				}
			}
			
			/* Way found */
			/*Dirty cell manage*/
		
			if(cache_blocks[idx*associativity+way].dirty == true)
			{
				//DIRTY_EVICTION = DIRTY_EVICTION+1;
				operation_result->dirty_eviction = 1;
				operation_result->evicted_address = cache_blocks[idx*associativity+way].tag;
			}
			
			/*Replacement*/
			cache_blocks[idx*associativity+way].rp_value = 0;
			cache_blocks[idx*associativity+way].tag = tag;
			cache_blocks[idx*associativity+way].valid = 1;
			cache_blocks[idx*associativity+way].dirty = 1;					
		}				
	return ERROR;
}

// ------------- END - RAMIREZ, JONATHAN ----------------
