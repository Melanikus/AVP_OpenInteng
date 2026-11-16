// The test of the link tree's branch walks (src/host/module_links.cpp) against inteng's own, read on the new dumps (AvP-docs\engine\inteng\reassess\module-linking-verify-dietrich.md
// rows ML-V-1..5, listing-all-2 0x4c4230-0x4c4585). Only the tree: no level, no game. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code".
//
// The case that tells inteng's Branch Link apart (ML-V-5): Branch Link (FUN_004c42b0) ends with Open Branch FROM THE ROOT on the node that got the branch (0x4c4329-0x4c4335:
// FUN_004c43c0 with ECX = the root), so a branch left open on the way (here 1's, opened by View after a Close) is closed when the new branch is met further along the chain.

#include <stdio.h>

#include <string>
#include <vector>

#include "module_links.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Walked(host::LinkTree const &tree)
{
	std::string out;

	for (host::LinkTree::Tint const &one : tree.Tints())
	{
		char part[32];

		snprintf(part, sizeof(part), "%s%d", out.empty() ? "" : " ", (int)one.module);
		out += part;
	}

	return out;
}

// The tree's items as FUN_004c46c0 writes them (the module stands for its object index): "module/branch_no", pre-order, closed branches too
static std::string Items(host::LinkTree const &tree)
{
	std::string out;

	for (host::LinkItem const &item : tree.Flatten())
	{
		out += (out.empty() ? "" : " ") + std::to_string(item.objectIndex) + "/" + std::to_string(item.branchNo);
	}

	return out;
}

int main()
{
	// R(100) -> 1 (branch: 2); 1's branch closed; 3 after 1; 1's branch viewed again (open, still marked closed); then a Branch Link: it goes on 3 (the chain passes 1, whose branch is closed)
	{
		host::LinkTree tree(100);

		Check(tree.Normal(1) && tree.Branch(2), "R -> 1, then 1 gets the branch 2");
		Check(tree.Close(1), "Close Branch on 1");
		Check(tree.Normal(3), "3 is linked after 1 (1's branch is closed)");
		Check(tree.View(1), "View on 1 opens its branch again (open, still marked closed)");
		Check(Walked(tree) == "100 1 2 3", "the walk enters 1's open branch: " + Walked(tree));
		Check(tree.Branch(4), "a Branch Link: 3 gets the branch 4");
		// FUN_004c43d0 from the root on 3: met in the chain after 1, so 1's open branch is closed (0x4c4440-0x4c445c): the walk no longer enters it
		Check(Walked(tree) == "100 1 3 4", "1's branch is closed by the Branch Link's Open Branch from the root: " + Walked(tree));
	}

	// Open Branch (ML-V-2) on a module met in an open branch: the nodes after the ancestor are closed
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Branch(2);   // 1 -> [2]
		tree.Normal(5);   // into 1's open branch: 2 -> 5
		tree.Close(1);
		tree.Normal(3);   // after 1
		tree.Branch(6);   // 3 -> [6]
		Check(Walked(tree) == "100 1 3 6", "1's branch closed, 3's open: " + Walked(tree));
		Check(tree.Open(1), "Open Branch on 1");
		// FUN_004c43d0: 1 is met with a branch that was closed: open, not closed, and every node after it (3 and its branch) closed (FUN_004c4560 on its next, 0x4c4420)
		Check(Walked(tree) == "100 1 2 5 3", "1's branch opened, 3's closed: " + Walked(tree));
	}

	// ML-V-12, Add Reverse Links' body (FUN_004c60a0 0x4c60ef-0x4c626e, listing-all-2): a path element found WITH a branch moves into the branch and leaves the
	// "found or made" flag (EBX) as it was (0x4c624a-0x4c6253 jumps past 0x4c6239's MOV EBX,1). While it is still clear, the next element is looked for along the
	// branch's own chain (0x4c60f7-0x4c6146), and a new node goes on as the NEXT of the node held (0x4c61e5-0x4c6234), not as its branch.
	{
		// C(3) -> 1 (branch: 2); the path {4, 1} walked from its last element: 1 is found with a branch, so 2 is held, the flag still clear; 4 is not 2, so 4
		// is made as 2's next: 1's branch becomes 2 -> 4 (items 1/3 2/0 4/0, FUN_004c46c0's numbers), not 2 with the branch 4 (1/3 2/3 4/0)
		host::LinkTree tree(3);

		tree.Normal(1);
		tree.Branch(2);
		tree.AddReversePath(std::vector<int32_t>({ 4, 1 }));
		Check(Items(tree) == "1/3 2/0 4/0", "a new node after a found branch goes on as the next in that branch: " + Items(tree));
	}

	{
		// C(3) -> 1 (branch: 2 -> 5); the path {6, 5, 1}: 1 found with a branch (2 held, flag clear), 5 looked for along 2's chain and found there with no
		// branch (the flag set), 6 made as 5's branch: 1 -> [2 -> 5 -> [6]] (items 1/4 2/0 5/4 6/0); not a second 5 made under 2
		host::LinkTree tree(3);

		tree.Normal(1);
		tree.Branch(2);
		tree.Normal(5);   // into 1's open branch: 2 -> 5
		Check(Items(tree) == "1/3 2/0 5/0", "the tree to start from: 1 -> [2 -> 5]: " + Items(tree));
		tree.AddReversePath(std::vector<int32_t>({ 6, 5, 1 }));
		Check(Items(tree) == "1/4 2/0 5/4 6/0", "an element after a found branch is looked for along the branch's chain: " + Items(tree));
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
