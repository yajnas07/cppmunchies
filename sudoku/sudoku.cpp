//====================================================================
//! @file Sudoku_solver.cpp
//! @brief Source file implementing a sudoku solver algorithm
//!
//! @author  Sanjay Singh , email.yajnas@gmail.com
//!
//! @copyright www.codepuz.com, Sanjay Singh
//====================================================================
#include<iostream>
#include<list>
#include<vector>
using namespace std;

//A sample Sudo acting as input to the algorithm.
//Change this to any other valid Sudoku for a different input. 
//Recompile and run to get the solution.
unsigned int A[9][9] = {
	                    5,0,0,9,0,0,6,0,0,
						0,0,0,0,0,0,0,0,2,
						9,1,7,0,6,2,0,3,5,
						0,0,0,3,0,0,0,5,6,
						3,0,2,6,0,8,9,0,4,
						7,4,0,0,0,5,0,0,0,
						2,7,0,5,4,0,3,8,1,
						1,0,0,0,0,0,0,0,0,
						0,0,3,0,0,9,0,0,7
}; 

class solved_entry_t
{
	
public:
	unsigned int i;
	unsigned int j;
	unsigned int value;
	vector<unsigned int> reject_list;
	solved_entry_t()
	{
		i=j=value=0;
	}
	solved_entry_t(unsigned int ii,unsigned int jj, unsigned int vv)
	{
		i = ii;
		j = jj;
		value = vv;
	}

};

bool present_in_cur_row(unsigned int A[9][9],unsigned int row,unsigned int Value)
{

	unsigned int i;
	for(i=0;i<9;i++)
	{
		if(A[row][i] == Value)
			return true;
	}
	return false;

}

bool present_in_cur_col(unsigned int A[9][9],unsigned int col,unsigned int Value)
{
	unsigned int i;
	for(i=0;i<9;i++)
	{
		if(A[i][col] == Value)
			return true;
	}
return false;

}

bool present_in_cur_block(unsigned int A[9][9],unsigned int i, unsigned int j,unsigned int Value)
{
	int k,l;
	int block_start_i = (i/3)*3;
	int	block_start_j = (j/3)*3;
	for(k = 0; k< 3; k++)
	{
		for(l=0;l<3;l++)
		{
			if(A[block_start_i+k][block_start_j+l] == Value)
				return true;

		}
	}
	return false;

}

//Finds all the possible values for a given node based on it reject list and other entries.
unsigned int find_possible_value(unsigned int A[9][9],solved_entry_t * node)
{
	//run a lop fron 1 to 9
	unsigned int k,l;
	for(k=1;k<=9;k++)
	{

		//If its present in current row continue
		if(present_in_cur_row(A,node->i,k))
		{
			continue;
		}
	
		//If its present in current col continue
		if(present_in_cur_col(A,node->j,k))
		{
			continue;
		}

		//If it is present in current block continus
		if(present_in_cur_block(A,node->i,node->j,k))
		{
			continue;
		}
		
		//Element should not be in reject list..
		bool valid = true;
		for(l=0;l<node->reject_list.size();l++)
		{
			if(k == node->reject_list[l])
			{
				valid = false;
				break;
			}
		}
		if(valid)
		{
		return k;
		}
		else
		{
			continue;
		}

	}
	//No value found
	return 0;
}

void print_sudoku(unsigned int A[9][9])
{


	unsigned int i,j;
	printf("\nPrinting Sudoku..\n");
	for(i=0;i<9;i++)
	{
		for(j=0;j<9;j++)
		{
			if(A[i][j])
			printf("%d, ",A[i][j]);
			else
			printf("_, ");
			if((j+1)%3 == 0)
			{
			printf(" ");
			}
		}
	
		printf("\n");
		if((i+1)%3 == 0)
		{
	    	printf("\n");
		}

	}
}

void reset_reject_list(vector<solved_entry_t*> * que_handle,vector<solved_entry_t*>::iterator que_iterator)
{
	vector<solved_entry_t*>::iterator it = que_iterator;
	for(;it<que_handle->end();it++)
	{
		(*it)->reject_list.clear();
	}

}

int main()
{

	//A stack to be used for string the moves and can be poped out for back tracking
	list<solved_entry_t*> filled_stack;

	//Hold the node that are not filled in yest
	vector<solved_entry_t*> empty_que;

	unsigned int i,j;
	print_sudoku(A);

	
	//Find the unfilled node and create an empty list accordingly
	for(i=0;i<9;i++)
	{

		for(j=0;j<9;j++)
		{
			if(A[i][j] == 0)
			{
				solved_entry_t * cur_node = new solved_entry_t(i,j,0);
				empty_que.push_back(cur_node);
			}
		}
	}
	

	vector<solved_entry_t*>::iterator que_iterator;
	//Loog through all the entries in the empty que
	for(que_iterator=empty_que.begin();que_iterator<empty_que.end();que_iterator++)
	{

		solved_entry_t * cur_node = *(que_iterator);

		//Find a possibe value that can be filled at the current empty node
		int value = find_possible_value(A,cur_node);
		if(0 != value)
		{
			//If a valid value exist then fill in the value and do appropirate book keeping.		
			A[cur_node->i][cur_node->j] = value;
			cur_node->value = value;
			filled_stack.push_back(cur_node);
			printf("Filled value %d at i=%d,j=%d..",value,cur_node->i,cur_node->j);
			print_sudoku(A);
			//Reset the reject list of all the nodes lying ahead of current node in the empty list
			reset_reject_list(&empty_que,(que_iterator+1));


		}
		else
		{
			//No valid value found that can be filled at current empty location.		           
			printf("Back tracking @i=%d,j=%d\n",cur_node->i,cur_node->j);
			solved_entry_t * last_node = filled_stack.back();
			filled_stack.pop_back();
			//Add the value tried to reject liast as this cannot be filled in the current place.
			last_node->reject_list.push_back(last_node->value);
			last_node->value = 0;
			A[last_node->i][last_node->j] = 0;
			print_sudoku(A);
			que_iterator--;
			if(que_iterator == empty_que.begin())
			{
				printf("Reached the start of trial list. Trying another value..\n");
				solved_entry_t * start_node = *que_iterator;
				unsigned int new_start_value = find_possible_value(A,start_node);
				if(new_start_value)
				{
				printf("Filled value %d at i=%d,j=%d..",new_start_value,start_node->i,start_node->j);
				A[start_node->i][start_node->j] = new_start_value;
				start_node->value = new_start_value;
				filled_stack.push_back(start_node);
				reset_reject_list(&empty_que,(que_iterator+1));
				print_sudoku(A);
				}
				else
				{
				printf("No solution possible..:(\n");
				exit(-1);
				}
			}
			else
			{
			que_iterator--;
			}

		}
	
	}
	printf("Sudoku Solved. Printing the final solution!!");
	print_sudoku(A);
	return 0;
}

