//-----------------------------------------------------------------------------
//   toposort.c
//
//   Project:  EPA SWMM5
//   Version:  5.2
//   Date:     11/01/21   (Build 5.2.0)
//   Author:   L. Rossman
//
//   Topological sorting of conveyance network links
//-----------------------------------------------------------------------------
#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include "headers.h"

//-----------------------------------------------------------------------------
//  Constants
//-----------------------------------------------------------------------------
enum AdjListType {UNDIRECTED, DIRECTED};    // type of nodal adjacency list

//-----------------------------------------------------------------------------
//  Shared variables
//-----------------------------------------------------------------------------
                  // number of incoming links to each node
                  // start of a node's outlinks in AdjList
                   // list of outlink indexes for each node
                     // array of nodes "reached" during sorting
                     // position of first node in stack
                      // position of last node added to stack

                 // TRUE if node included in spanning tree
                   // state of each link in spanning tree:
                                       // 0 = unexamined,
                                       // 1 = in spanning tree,
                                       // 2 = chord of spanning tree
                // list of links which forms a loop

// ---- toposort module state: formerly file-scope variables of toposort.c.
// ---- One instance per Project, so two models never share it.
struct ToposortState
{
    int * InDegree{};   // was static
    int * StartPos{};   // was static
    int * AdjList{};   // was static
    int * Stack{};   // was static
    int First{};   // was static
    int Last{};   // was static
    char * Examined{};   // was static
    char * InTree{};   // was static
    int * LoopLinks{};   // was static
    int LoopLinksLast{};   // was static
};
void Project::toposort_createState() { toposort_ = new ToposortState(); }
void Project::toposort_destroyState() { delete toposort_; toposort_ = nullptr; }
            // number of links in a loop

//-----------------------------------------------------------------------------
//  External functions (declared in funcs.h)   
//-----------------------------------------------------------------------------
//  toposort_sortLinks (called by routing_open)

//-----------------------------------------------------------------------------
//  Local functions
//-----------------------------------------------------------------------------








//=============================================================================

void Project::toposort_sortLinks(int sortedLinks[])
//
//  Input:   none
//  Output:  sortedLinks = array of link indexes in sorted order
//  Purpose: sorts links from upstream to downstream.
//
{
    int i, n = 0;

    // --- no need to sort links for Dyn. Wave routing
    for ( i=0; i<Nobjects[LINK]; i++) sortedLinks[i] = i;
    if ( RouteModel == DW )
    {

        // --- check for nodes with both incoming and outgoing
        //     dummy links (creates ambiguous ordering)
        checkDummyLinks();
        if ( ErrorCode ) return;

        // --- find number of outflow links for each node
        for ( i=0; i<Nobjects[NODE]; i++ ) Node[i].degree = 0;
        for ( i=0; i<Nobjects[LINK]; i++ )
        {
            // --- if upstream node is an outfall, then increment outflow
            //     count for downstream node, otherwise increment count
            //     for upstream node
            n = Link[i].node1;
            if ( Link[i].direction < 0 ) n = Link[i].node2;
            if ( Node[n].type == OUTFALL )
            {
                if ( Link[i].direction < 0 ) n = Link[i].node1;
                else n = Link[i].node2;
                Node[n].degree++;
            }
            else Node[n].degree++;
        }
        return;
    }

    // --- allocate arrays used for topo sorting
    if ( ErrorCode ) return;
    toposort_->InDegree = (int *) calloc(Nobjects[NODE], sizeof(int));
    toposort_->StartPos = (int *) calloc(Nobjects[NODE], sizeof(int));
    toposort_->AdjList  = (int *) calloc(Nobjects[LINK], sizeof(int));
    toposort_->Stack    = (int *) calloc(Nobjects[NODE], sizeof(int));
    if ( toposort_->InDegree == NULL || toposort_->StartPos == NULL ||
         toposort_->AdjList == NULL || toposort_->Stack == NULL )
    {
        report_writeErrorMsg(ERR_MEMORY, "");
    }
    else
    {
        // --- create a directed adjacency list of links leaving each node
        createAdjList(DIRECTED);

        // --- adjust adjacency list for DIVIDER nodes
        adjustAdjList();

        // --- find number of links entering each node
        for (i = 0; i < Nobjects[NODE]; i++) toposort_->InDegree[i] = 0;
        for (i = 0; i < Nobjects[LINK]; i++)
        {
            toposort_->InDegree[ Link[i].node2 ]++;
        }

        // --- topo sort the links
        n = topoSort(sortedLinks);
    }   

    // --- free allocated memory
    FREE(toposort_->InDegree);
    FREE(toposort_->StartPos);
    FREE(toposort_->AdjList);
    FREE(toposort_->Stack);

    // --- check that all links are included in SortedLinks
    if ( !ErrorCode &&  n != Nobjects[LINK] )
    {
        report_writeErrorMsg(ERR_LOOP, "");
        findCycles();
    }
}

//=============================================================================

void Project::createAdjList(int listType)
//
//  Input:   lsitType = DIRECTED or UNDIRECTED
//  Output:  none
//  Purpose: creates listing of links incident on each node.
//
{
    int i, j, k;

    // --- determine degree of each node
    //     (for DIRECTED list only count link at its upstream node;
    //      for UNDIRECTED list count link at both end nodes)
    for (i = 0; i < Nobjects[NODE]; i++) Node[i].degree = 0;
    for (j = 0; j < Nobjects[LINK]; j++)
    {
        Node[ Link[j].node1 ].degree++;
        if ( listType == UNDIRECTED ) Node[ Link[j].node2 ].degree++;
    }

    // --- determine start position of each node in the adjacency list
    //     (the adjacency list, AdjList, is one long vector containing
    //      the individual node lists one after the other)
    toposort_->StartPos[0] = 0;
    for (i = 0; i < Nobjects[NODE]-1; i++)
    {
        toposort_->StartPos[i+1] = toposort_->StartPos[i] + Node[i].degree;
        Node[i].degree = 0;
    }
    Node[Nobjects[NODE]-1].degree = 0;

    // --- traverse the list of links once more,
    //     adding each link's index to the proper 
    //     position in the adjacency list
    for (j = 0; j < Nobjects[LINK]; j++)
    {
        i = Link[j].node1;
        k = toposort_->StartPos[i] + Node[i].degree;
        toposort_->AdjList[k] = j;
        Node[i].degree++;
        if ( listType == UNDIRECTED )
        {
            i = Link[j].node2;
            k = toposort_->StartPos[i] + Node[i].degree;
            toposort_->AdjList[k] = j;
            Node[i].degree++;
        }
    }
}

//=============================================================================

void Project::adjustAdjList()
//
//  Input:   none
//  Output:  none
//  Purpose: adjusts adjacency list for Divider nodes so that non-
//           diversion link appears before diversion link.
//
{
    int i, j, k, m;

    // --- check each node
    for (i=0; i<Nobjects[NODE]; i++)
    {
        // --- skip nodes that are not Dividers
        if ( Node[i].type != DIVIDER ) continue;
        if ( Node[i].degree != 2 ) continue;

        // --- switch position of outgoing links at the node if the
        //     diversion link appears first in the adjacency list
        k = Node[i].subIndex;
        m = toposort_->StartPos[i];
        j = toposort_->AdjList[m];
        if ( j == Divider[k].link )
        {
            toposort_->AdjList[m] = toposort_->AdjList[m+1];
            toposort_->AdjList[m+1] = j;
        }
    }
}

//=============================================================================

int Project::topoSort(int sortedLinks[])
//
//  Input:   none
//  Output:  sortedLinks = array of sorted link indexes,
//           returns number of links successfully sorted
//  Purpose: performs a stack-based topo sort of the drainage network's links.
//
{
    int i, j, k, n;
    int i1, i2, k1, k2;

    // --- initialize a stack which contains nodes with zero in-degree
    toposort_->First = 0;
    toposort_->Last = -1;
    for (i = 0; i < Nobjects[NODE]; i++)
    {
        if ( toposort_->InDegree[i] == 0 )
        {
            toposort_->Last++;
            toposort_->Stack[toposort_->Last] = i;
        }
    }

    // --- traverse the stack, adding each node's outgoing link indexes
    //     to the SortedLinks array in the order processed
    n = 0;
    while ( toposort_->First <= toposort_->Last )
    {
        // --- determine range of adjacency list indexes belonging to 
        //     first node remaining on the stack
        i1 = toposort_->Stack[toposort_->First];
        k1 = toposort_->StartPos[i1];
        k2 = k1 + Node[i1].degree;

        // --- for each outgoing link from first node on stack
        for (k = k1; k < k2; k++)
        {
            // --- add link index to current position in SortedLinks
            j = toposort_->AdjList[k];
            sortedLinks[n] = j;
            n++;

            // --- reduce in-degree of link's downstream node
            i2 = Link[j].node2;
            toposort_->InDegree[i2]--;
            // --- add downstream node to stack if its in-degree is zero
            if ( toposort_->InDegree[i2] == 0 )
            {
                toposort_->Last++;
                toposort_->Stack[toposort_->Last] = i2;
            }  
        }
        toposort_->First++;
    }
    return n;
}

//=============================================================================

void  Project::findCycles()
//
//  Input:   none
//  Output:  none
//  Purpose: finds all cycles in the drainage network (i.e., closed loops that
//           start and end at the same node).
//
{
    int i;

    // --- allocate arrays
    toposort_->AdjList  = (int *) calloc(2*(size_t)Nobjects[LINK], sizeof(int));
    toposort_->StartPos = (int *) calloc(Nobjects[NODE], sizeof(int));
    toposort_->Stack    = (int *) calloc(Nobjects[NODE], sizeof(int));
    toposort_->Examined = (char *) calloc(Nobjects[NODE], sizeof(char));
    toposort_->InTree   = (char *) calloc(Nobjects[LINK], sizeof(char));
    toposort_->LoopLinks = (int *) calloc(Nobjects[LINK], sizeof(int));
    if ( toposort_->StartPos && toposort_->AdjList && toposort_->Stack && toposort_->Examined && toposort_->InTree && toposort_->LoopLinks )
    {
        // --- create an undirected adjacency list for the nodes
        createAdjList(UNDIRECTED);

        // --- set to empty the list of nodes examined and the list
        //     of links in the spanning tree
        for ( i=0; i<Nobjects[NODE]; i++) toposort_->Examined[i] = 0;
        for ( i=0; i<Nobjects[LINK]; i++) toposort_->InTree[i] = 0;

        // --- find a spanning tree for each unexamined node
        //     (cycles are identified as tree is constructed)
        for ( i=0; i<Nobjects[NODE]; i++)
        {
            if ( toposort_->Examined[i] ) continue;
            toposort_->Last = -1;
            findSpanningTree(i);
        }
    }
    FREE(toposort_->StartPos);
    FREE(toposort_->AdjList);
    FREE(toposort_->Stack);
    FREE(toposort_->Examined);
    FREE(toposort_->InTree);
    FREE(toposort_->LoopLinks);
}

//=============================================================================

void  Project::findSpanningTree(int startNode)
//
//  Input:   i = index of starting node of tree
//  Output:  none
//  Purpose: finds continuation of network's spanning tree of links.
//
{
    int nextNode, j, k, m;

    // --- examine each link connected to node i
    for ( m = toposort_->StartPos[startNode];
          m < toposort_->StartPos[startNode]+Node[startNode].degree; m++ )
    {
        // --- find which node (j) connects link k from start node
        k = toposort_->AdjList[m];
        if ( Link[k].node1 == startNode ) j = Link[k].node2;
        else j = Link[k].node1;

        // --- if link k is not in the tree
        if ( toposort_->InTree[k] == 0 )
        {
            // --- if connecting node already examined,
            //     then link k forms a loop; mark it as a chord
            //     and check if loop forms a cycle
            if ( toposort_->Examined[j] )
            {
                toposort_->InTree[k] = 2;
                evalLoop(k);
            }

            // --- otherwise mark connected node as being examined,
            //     add it to the node stack, and mark the connecting
            //     link as being in the spanning tree
            else
            {
                toposort_->Examined[j] = 1;
                toposort_->Last++;
                toposort_->Stack[toposort_->Last] = j;
                toposort_->InTree[k] = 1;
            }
        }
    }

    // --- continue to grow the spanning tree from
    //     the last node added to the stack
    if ( toposort_->Last >= 0 )
    {
        nextNode = toposort_->Stack[toposort_->Last];
        toposort_->Last--;
        findSpanningTree(nextNode);
    }
}

//=============================================================================

void Project::evalLoop(int startLink)
//
//  Input:   startLink = index of starting link of a loop
//  Output:  none
//  Purpose: checks if loop starting with a given link forms a closed cycle.
//
{
    int i;                             // loop list index
    int i1, i2;                        // start & end node indexes
    int j;                             // index of link in loop
    int kount;                         // items per line counter
    int isCycle;                       // TRUE if loop forms a cycle

    // --- make startLink the first link in the loop
    toposort_->LoopLinksLast = 0;
    toposort_->LoopLinks[0] = startLink;

    // --- trace a path on the spanning tree that starts at the
    //     tail node of startLink and ends at its head node
    i1 = Link[startLink].node1;
    i2 = Link[startLink].node2;
    if ( !traceLoop(i1, i2, startLink) ) return;

    // --- check if all links on the path are oriented head-to-tail
    isCycle = TRUE;
    j = toposort_->LoopLinks[0];
    i2 = Link[j].node2;
    for (i=1; i<=toposort_->LoopLinksLast; i++)
    {
        j = toposort_->LoopLinks[i];
        i1 = Link[j].node1;
        if ( i1 == i2 ) i2 = Link[j].node2;
        else
        {
            isCycle = FALSE;
            break;
        }
    }

    // --- print cycle to report file
    if ( isCycle )
    {
        kount = 0;
        for (i = 0; i <= toposort_->LoopLinksLast; i++)
        {
            if ( kount % 5 == 0 ) fprintf(Frpt.file, "\n");
            kount++;
            fprintf(Frpt.file, "  %s", Link[toposort_->LoopLinks[i]].ID);
            if ( i < toposort_->LoopLinksLast ) fprintf(Frpt.file, "  -->");
        }
    }
}

//=============================================================================

int Project::traceLoop(int i1, int i2, int k1)
//
//  Input:   i1 = index of current node reached while tracing a loop
//           i2 = index of final node on the loop
//           k1 = index of link which extends loop to node i1
//  Output:  returns TRUE if loop can be extended through node i1
//  Purpose: tries to extend closed loop through current node.
//
{
    int j, k, m;

    // --- if current node equals final node then return with loop found
    if ( i1 == i2 ) return TRUE;

    // --- examine each link connected to current node
    for (m = toposort_->StartPos[i1]; m < toposort_->StartPos[i1] + Node[i1].degree; m++)
    {
        // --- ignore link if it comes from predecessor node or if
        //     it is not on the spanning tree
        k = toposort_->AdjList[m];
        if ( k == k1 || toposort_->InTree[k] != 1 ) continue;

        // --- identify other node that link is connected to
        if ( Link[k].node1 == i1 ) j = Link[k].node2;
        else                       j = Link[k].node1;

        // --- try to continue tracing the loop from this node;
        //     if successful, then add link to loop and return
        if ( traceLoop(j, i2, k) )
        {
            toposort_->LoopLinksLast++;
            toposort_->LoopLinks[toposort_->LoopLinksLast] = k;
            return TRUE;
        }
    }

    // --- return false if loop cannot be continued from current node
    return FALSE;
}

//=============================================================================

void Project::checkDummyLinks()
//
//  Input:   none
//  Output:  none
//  Purpose: checks for nodes that have both incoming and outgoing dummy links.
//
{
    int   i, j;
    int* marked;

    // --- create an array that marks nodes
    //     (calloc initializes the array to 0)
    marked = (int *) calloc(Nobjects[NODE], sizeof(int));
    if ( marked == NULL )
    {
        report_writeErrorMsg(ERR_MEMORY, "");
        return;
    }

    // --- mark nodes that whose incoming links are all
    //     either dummy links or ideal pumps
    for ( i = 0; i < Nobjects[LINK]; i++ )
    {
        j = Link[i].node2;
        if ( Link[i].direction < 0 ) j = Link[i].node1;
        if ( (Link[i].type == CONDUIT && Link[i].xsect.type == DUMMY) ||
             (Link[i].type == PUMP &&
              Pump[Link[i].subIndex].type == IDEAL_PUMP) )
        {
            if ( marked[j] == 0 ) marked[j] = 1;
        }
        else marked[j] = -1;
    }

    // --- find marked nodes with outgoing dummy links or ideal pumps
    for ( i = 0; i < Nobjects[LINK]; i++ )
    {
        if ( (Link[i].type == CONDUIT && Link[i].xsect.type == DUMMY) ||
             (Link[i].type == PUMP && 
              Pump[Link[i].subIndex].type == IDEAL_PUMP) )
        {
            j = Link[i].node1;
            if ( marked[j] > 0 )
            {
                report_writeErrorMsg(ERR_DUMMY_LINK, Node[j].ID);
            }
        }
    }
    FREE(marked);
}

//=============================================================================
