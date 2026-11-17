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
		Check(tree.OpenBranches() == std::vector<int32_t>({ 1 }), "the open branches a screenshot names (#78): 1 only, 3's closed one is not shown");
		tree.Hide(1);
		Check(tree.OpenBranches().empty(), "after Hide on 1: no branch is shown open");
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

	// ML-A-7, AutoLink's insertion (FUN_004c6fb0): NOT Normal's walk. R(100) -> 1 (branch 2, not closed) -> 3
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Branch(2);   // 1 -> [2], open
		tree.Close(1);
		tree.Normal(3);   // after 1
		tree.View(1);     // 1's branch open again, still marked closed: open it fully with Open
		tree.Open(1);     // 1's branch not closed; 3 (after it) closed: it has no branch, so nothing changes for it
		Check(Items(tree) == "1/2 2/0 3/0", "the tree to start from: 1 -> [2], then 3: " + Items(tree));

		host::LinkTree::AutoLinkPicture picture = tree.AutoLinkBegin(-1);
		// the walk goes along the whole chain (1, 3) first: 3 is met, so it is not added again (Normal would go into 1's branch first, miss 3, and add it a second time)
		Check(!tree.AutoLinkAdd(&picture, 3), "a module on the walked chain is not added again");
		// a new one goes after the end of the last not-closed branch's chain: 2, inside 1's branch
		Check(tree.AutoLinkAdd(&picture, 4), "a new module is added");
		tree.AutoLinkEnd(picture);
		Check(Items(tree) == "1/3 2/0 4/0 3/0", "it went after 2, inside 1's branch: " + Items(tree));
	}

	// ML-A-7 (c), a DOOR's second picture: R(100) -> 1 -> 5 (the door) -> 6; the new modules become 5's branch, which is opened, then closed at the end (inteng's red / green door)
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Normal(5);
		tree.Normal(6);
		host::LinkTree::AutoLinkPicture picture = tree.AutoLinkBegin(5);
		Check(tree.AutoLinkAdd(&picture, 7), "the first module seen through the door is added");
		Check(tree.AutoLinkAdd(&picture, 8), "and the next");
		Check(!tree.AutoLinkAdd(&picture, 6), "a module already on the chain is not added again");
		tree.AutoLinkEnd(picture);
		Check(Items(tree) == "1/0 5/4 7/0 8/0 6/0", "they became the door's branch, in order: " + Items(tree));

		bool red = false;

		for (host::LinkTree::Tint const &one : tree.Tints())
		{
			red = red || (one.module == 5 && one.rgb == 0xFF0000);
		}

		Check(red, "the door's branch is closed at the end: the door is drawn red");
	}

	// ML-A-7, NESTED door pictures (FUN_004c6fb0 calls itself at 0x4c785c after its own insertions and BEFORE its own Close at 0x4c7952): R(100) -> 1 -> 5 (door A); A's second picture
	// sees 7 and door B (9); B's second picture (inside A's) sees 11. Each picture keeps its own door node and branch flag, so B's Close does not end A's, and A closes last.
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Normal(5);

		host::LinkTree::AutoLinkPicture a = tree.AutoLinkBegin(5);

		tree.AutoLinkAdd(&a, 7);
		tree.AutoLinkAdd(&a, 9);   // the door B, seen through A, goes into A's branch

		host::LinkTree::AutoLinkPicture b = tree.AutoLinkBegin(9);

		Check(tree.AutoLinkAdd(&b, 11), "the module seen through B is added");
		tree.AutoLinkEnd(b);
		// FUN_004c46c0's numbers: an item with a branch holds the place of the first item after it (5 at place 2 with 7, 9, 11 under it: 5; 9 at place 4 with 11: 5)
		Check(Items(tree) == "1/0 5/5 7/0 9/5 11/0", "11 is B's branch, inside A's: " + Items(tree));
		Check(a.door == 5 && a.branchMade, "A's own state is kept while B's picture runs");
		tree.AutoLinkEnd(a);

		bool fiveRed = false, green = false;

		for (host::LinkTree::Tint const &one : tree.Tints())
		{
			fiveRed = fiveRed || (one.module == 5 && one.rgb == 0xFF0000);
			green = green || ((one.module == 5 || one.module == 9) && one.rgb == 0x00FF00);
		}

		Check(fiveRed && !green, "A's branch is closed at the end, after B's: 5 red, neither door green");
	}

	// ML-A-7, a door picture that REUSES the door's branch: AutoLinkBegin un-closes and opens it (0x4c7023) and counts it as made, so AutoLinkEnd closes it again. (A bad pixel
	// leaves inteng's FUN_004c6fb0 before its Close, 0x4c729a, so inteng left it open: INTENG BUG, FIXED in AutoLinkTreeWriter, host_autolink_run_test.)
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Normal(5);
		tree.Branch(6);   // 5 -> [6]
		tree.Close(5);

		host::LinkTree::AutoLinkPicture again = tree.AutoLinkBegin(5);

		Check(again.branchMade, "the door's branch is reused");

		bool green = false;

		for (host::LinkTree::Tint const &one : tree.Tints())
		{
			green = green || (one.module == 5 && one.rgb == 0x00FF00);
		}

		Check(green, "it is open while the picture runs (green)");
	}

	// ML-A-7, INTENG BUG, FIXED (MAPPING-ORDERS section 15): an OPEN branch BEFORE the door on the chain. inteng finds the door by Normal's walk (0x4c6fe8-0x4c702e: into the FIRST open
	// branch, never back), so it walks into 1's branch, misses 2, and hangs what is seen through door 2 on 11. The door is now found by the insertion's own walk (each whole chain first).
	// R(100) -> 1 -> 2; door 1's picture is still running (its branch [11] open), and inside it door 2's picture sees 22.
	{
		host::LinkTree tree(100);

		tree.Normal(1);
		tree.Normal(2);

		host::LinkTree::AutoLinkPicture one = tree.AutoLinkBegin(1);

		tree.AutoLinkAdd(&one, 11);   // 1 -> [11], open (Open Branch from 1)

		host::LinkTree::AutoLinkPicture two = tree.AutoLinkBegin(2);

		Check(two.doorNode != nullptr && tree.AutoLinkAdd(&two, 22), "the module seen through door 2 is added");
		// FUN_004c46c0's numbers: 1 at place 1 with [11]: 2; 2 at place 3 with [22]: 4 (inteng's walk would give 1/3 11/3 22/0 2/0: 22 under 11)
		Check(Items(tree) == "1/2 11/0 2/4 22/0", "22 is door 2's branch, not 11's: " + Items(tree));
		tree.AutoLinkEnd(two);
		tree.AutoLinkEnd(one);
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
