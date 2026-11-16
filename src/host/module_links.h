// module_links.h: a module's links as inteng keeps them while Module Linking edits them (a tree of nodes), loaded from the module's VMDARRAY
// items and written back as items, EXACTLY as inteng does. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P5, links). AVP_OpenInteng's
// own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHERE EACH PART COMES FROM (re-tools\projects\vasquez-inteng\out; every step below is the decompile's, not a guess):
// - the node: 0x1c bytes, made by FUN_004c41d0 (menu-reads-4.txt): [0] the module, [1] the next node in the chain, [2] the node's branch (the
//   first node of a chain under it), [3] "closed", [4] "open", [5] set only for the root, [6] "changed". A new node has all but the module 0.
// - Normal Link FUN_004c4230, Branch Link FUN_004c42b0, Close Branch FUN_004c4350 (with FUN_004c4560), Delete Link FUN_004c4590 (menu-reads-2.txt);
// - loading the module's stored links FUN_004c10c0 -> FUN_004c40c0 -> FUN_004c40f0 (menu-reads-12.txt): items in pre-order, an item's branch_no
//   being the place of the first item after its branch, so a branch holds branch_no - place - 1 items;
// - writing them back FUN_004c46a0 / FUN_004c46c0 (menu-reads-4.txt): item[0] is the branch's node count plus the item's 1-based place (0 for no
//   branch), flags and spare 0, item[3] the module's object index. So open/closed state is NOT saved: a level read again has every branch closed.
// The links of the root node (the module being edited) hang off root->next. Nothing here reads or writes a level: the editor maps modules to object
// indexes and VMDARRAY chunks.
//
// Open Branch, View and Hide are built and read again on the new dumps (2026-11-16, MAPPING-ORDERS section 14): reassess/module-linking-verify-dietrich.md
// ML-V-1..4. Open Branch calls FUN_004c4560 on the node's next (0x4c4420) and on its branch (0x4c445c); View / Hide never call it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

namespace host
{
	// One item of a VMDARRAY, in the game's own struct order (VMod_Arr_Item, chunk_type.hpp)
	struct LinkItem
	{
		int32_t branchNo;
		int32_t flags;
		int32_t spare;
		int32_t objectIndex;
	};

	class LinkTree
	{
	public:
		// A tree with only the root: the module being edited
		explicit LinkTree(int32_t rootModule);
		~LinkTree();

		LinkTree(LinkTree const &) = delete;
		LinkTree &operator=(LinkTree const &) = delete;

		// Normal Link (FUN_004c4230): gives false, changing nothing but the root's "changed", when the module is the root or is met on the way to
		// the last node (closed branches are not looked into); otherwise a node is added after the last one.
		bool Normal(int32_t module);

		// Branch Link (FUN_004c42b0): a branch is begun, open, under the last node that has none, or false (the root itself, or the last node's
		// branch is closed).
		bool Branch(int32_t module);

		// ADD REVERSE LINKS (FUN_004c60a0, menu-reads-4.txt; AvP-docs\engine\inteng\drake-autolink-facts.md section 1). ReverseVisits walks THIS tree as
		// FUN_004c60a0 does (the chain after the root, branches entered with the node's module appended to the path, the first element equal to it taken
		// out afterwards) and gives, for each node met, its module and the path at that moment (head to tail; it starts as { the root's module }).
		struct ReverseVisit
		{
			int32_t module;
			std::vector<int32_t> path;
		};

		void ReverseVisits(std::vector<ReverseVisit> *out) const;

		// The body of FUN_004c60a0 for the CURRENT tree (the one of a module that was a node of the tree walked) and one path (row ML-V-12, listing-all-2
		// 0x4c60c9-0x4c626e): for each element from the last to the first, while no node was found without a branch or made, the node of that module is
		// looked for along the chain of the node held (first the root's); found with a branch, the branch's first node is held next (and the next element
		// is looked for along ITS chain); found without one, it stays held; not found, a new node is made, as the branch of the node held once a node was
		// found without a branch or made, else as the next of the node held. A new node is made by FUN_004c41a0, which sets its [5] (the root flag) too.
		void AddReversePath(std::vector<int32_t> const &path);

		// View Branch (FUN_004c4480) and Hide Branch (FUN_004c44f0): the node of that module that has a branch gets its "open" on (View) or off (Hide);
		// only open branches are searched. False when no such node is met. They change nothing else (not "changed": the open state is not saved).
		bool View(int32_t module);
		bool Hide(int32_t module);

		// Open Branch (FUN_004c43d0): the module's branch is opened and made not closed, and the branches that were open before it in the walk, and every later
		// node after it, are closed (see the .cpp for each step). False when the module is not met. Does not touch "changed".
		bool Open(int32_t module);

		// Close Branch (FUN_004c4350): the node of that module that has a branch, and every later sibling and their branches, are marked closed.
		// False for the root's own module.
		bool Close(int32_t module);

		// Delete Link (FUN_004c4590): the node of that module is taken out, with its own branch and chain, from inside an OPEN branch or the next
		// chain. Gives what inteng gives: false when it was found in a next chain (a quirk of FUN_004c4590: it removes and then returns 0).
		bool Delete(int32_t module);

		// The items of the tree, as FUN_004c46a0 writes them: the links of the root, in pre-order. objectIndexOf is FUN_004d0fc0 +0x98: the object
		// index of a node's module.
		std::vector<LinkItem> Flatten() const;

		// Loads items (FUN_004c40c0): exists(i) is FUN_004c4070, whether a module has that object index (a node is made only for one that does).
		void Load(LinkItem const *items, int count, std::function<bool(int32_t)> const &exists);

		// The root's "changed" is put on, as FUN_004c10c0 does for a tree it has just made again (`*(DAT_00704374 + 0x18) = 1`, 0x4c14df)
		void MarkChanged();

		// For tests and the editor: how many nodes there are besides the root, and whether a module is in the tree
		size_t Count() const;
		bool Has(int32_t module) const;

		// The module being edited (the root), whether the tree was changed since it was loaded (the root's "changed", [6]: Normal, Branch and Delete
		// set it; Close does not, as inteng's Close doesn't), whether the root has any link (root->next, FUN_004c10c0's `DAT_00704374[1] != 0`)
		int32_t RootModule() const;
		bool Changed() const;
		void ClearChanged();
		bool HasLinks() const;

		// The modules in the tree besides the root, in pre-order, closed branches too (for drawing them)
		std::vector<int32_t> Modules() const;

		// The colour of each module of the tree as inteng draws it (FUN_004c0720, menu-reads-4.txt:1891-1970, in true colour: DAT_00d876d8 != 0), as
		// 0xRRGGBB, root first then in the order inteng walks: the current module (the root, no branch) blue 0x0000FF; a link with no branch yellow
		// 0xFFFF00; a link with a branch green 0x00FF00 while the branch is not closed and red 0xFF0000 when it is. A branch is walked into only while
		// it is OPEN ([4]), the chain after a node always.
		struct Tint
		{
			int32_t module;
			uint32_t rgb;
		};

		std::vector<Tint> Tints() const;

	private:
		struct Node;

		static void FreeChain(Node *node);
		Node *root;
	};
}
