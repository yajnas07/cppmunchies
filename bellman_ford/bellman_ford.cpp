#include <climits>
#include <iostream>
#include <assert.h>

 #define INFINITY 999999999

 typedef struct {
        int source, destination, weight;
 } Edge;

void BellmanFord( Edge edges[], int edgenum, int nodes, int source)
 {
     int distance[32];
     if (distance == NULL) {
		 std::cout <<"Failed, exit program \n";
       exit(1);
     }//end if
int i;
     for ( i=0; i < nodes; ++i)
       distance[i] = INFINITY;

     distance[source] = 0;

     for (i=0; i < nodes; ++i){
         for (int j=0; j < edgenum; ++j){
             if (distance[edges[j].source] != INFINITY) {
                   if (distance[edges[j].source] + edges[j].weight < distance[edges[j].destination]){
                    int dummy = distance[edges[j].source] + edges[j].weight;
                        if (dummy < distance[edges[j].destination])
                        distance[edges[j].destination] = dummy;
                    }//end if
             }//end if
         }//end for
     }//end for

     for (i=0; i < edgenum; ++i) {
         if (distance[edges[i].destination] > distance[edges[i].source] + edges[i].weight) {
             std::cout<<"Negative edge weight cycles detected!\n";
             free(distance);
             break;
         }//end if
     }//end for

     for ( i=0; i < nodes; ++i) {
         std::cout<<"\nThe shortest distance between nodes " <<source<< " and " <<i<< " is " <<distance[i] <<"\n";
     }//end for
     //free(distance);
 }



int find_distance(Edge edges[],int source,int dest,int no_of_edges)
{

	int i;
	for(i=0;i<no_of_edges;i++)
	{
		if(edges[i].source == source && edges[i].destination == dest)
		{
		 return edges[i].weight;
		}
	}
	return INFINITY;
}

void print_graph( Edge edges[], int edgenum, int nodes)
{
int i,j;
int dest;
	for(i=0;i<nodes;i++)
	{
	printf("\t%c",'A'+i);
	}
	for(i=0;i<nodes;i++)
	{
		printf("\n%c",'A'+i);
		for(j=0;j<nodes;j++)
		{
         if(i == j)
		 {
		 dest = 0;
		 }
		 else
		 {
           dest=find_distance(edges,i,j,edgenum);
		 }
		 if( dest != INFINITY)
		 {
		 printf("\t%d",dest);
		 }
		 else
		 {
		 printf("\t%c",36);
		 }
		}

		

	}

}

 int main()
 {
  Edge * edge_list;
  /*edge_list[0].source = 0;
  edge_list[0].destination = 1;
  edge_list[0].weight  = -4;

    edge_list[1].source = 1;
  edge_list[1].destination = 2;
  edge_list[1].weight  = -1;

    edge_list[2].source = 0;
  edge_list[2].destination = 2;
  edge_list[2].weight  = -6;

    edge_list[3].source = 2;
  edge_list[3].destination = 3;
  edge_list[3].weight  = -8;
  */
   std::string file_name = "input.txt";
   FILE * input_file_fp;
   

   input_file_fp = fopen(file_name.c_str(),"r");
   if(input_file_fp == NULL)
   {
   printf("Could not open input file\n");
   return 1;
   }
   int no_of_edges,no_of_vertices;
   int i,j;
   
   fscanf(input_file_fp,"%d",&no_of_edges);
   assert(no_of_edges < 2500 && no_of_edges> 0);
   fscanf(input_file_fp,"%d",&no_of_vertices);
   assert(no_of_vertices < 50 && no_of_vertices> 0);

   edge_list = new Edge[no_of_edges];
   assert(edge_list);
    int temp;
   for(i = 0;i<no_of_edges;i++)
   {
	
	j++;
	fscanf(input_file_fp,"%d",&temp);
	edge_list[i].source = temp;
	fscanf(input_file_fp,"%d",&temp);
	edge_list[i].destination = temp;
	fscanf(input_file_fp,"%d",&temp);
	edge_list[i].weight = temp;

	}

   

    print_graph(edge_list,no_of_edges,no_of_vertices);
    BellmanFord(edge_list,no_of_edges,no_of_vertices,0);	 
	 return 0;
 }