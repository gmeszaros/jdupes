/* jdupes double-traversal prevention tree
 * See jdupes.c for license information */

#ifndef NO_TRAVCHECK

#include <stdlib.h>
#include <inttypes.h>

#include <libjodycode.h>
#include "jdupes.h"
#include "travcheck.h"

/* Simple traversal balancing hash - scrambles inode number */
#define TRAVHASH(device,inode) (((inode << 55 | (inode >> 9)) + (device << 13)))

static struct travcheck *travcheck_head = NULL;

/* Create a new traversal check object and initialize its values */
static struct travcheck *travcheck_alloc(const dev_t device, const jdupes_ino_t inode, uintmax_t hash)
{
	struct travcheck *trav;

	trav = (struct travcheck *)malloc(sizeof(struct travcheck));
	if (trav == NULL) return NULL;
	trav->left = NULL;
	trav->right = NULL;
	trav->hash = hash;
	trav->device = device;
	trav->inode = inode;
	return trav;
}


/* De-allocate the travcheck tree */
void travcheck_free(struct travcheck *cur)
{

	if (cur == NULL) {
		if (travcheck_head == NULL) return;
		cur = travcheck_head;
		travcheck_head = NULL;
	}
	if (cur->left == cur) goto error_travcheck_ptr;
	if (cur->right == cur) goto error_travcheck_ptr;
	if (cur->left != NULL) travcheck_free(cur->left);
	if (cur->right != NULL) travcheck_free(cur->right);
	if (cur != NULL) free(cur);
	return;
error_travcheck_ptr:
	fprintf(stderr, "internal error: invalid pointer in travcheck_free(), report this\n");
	exit(EXIT_FAILURE);
}


/* Check to see if device:inode pair has already been traversed */
int traverse_check(const dev_t device, const jdupes_ino_t inode)
{
	struct travcheck *traverse = travcheck_head;
	uintmax_t travhash;

	travhash = TRAVHASH(device, inode);
	if (travcheck_head == NULL) {
		travcheck_head = travcheck_alloc(device, inode, TRAVHASH(device, inode));
		if (travcheck_head == NULL) return 2;
	} else {
		traverse = travcheck_head;
		while (1) {
			/* Don't re-traverse directories we've already seen */
			if (inode == traverse->inode && device == traverse->device) {
				return 1;
			} else {
				if (travhash > traverse->hash) {
					/* Traverse right */
					if (traverse->right == NULL) {
						traverse->right = travcheck_alloc(device, inode, travhash);
						if (traverse->right == NULL) return 2;
						break;
					}
					traverse = traverse->right;
					continue;
				} else {
					/* Traverse left */
					if (traverse->left == NULL) {
						traverse->left = travcheck_alloc(device, inode, travhash);
						if (traverse->left == NULL) return 2;
						break;
					}
					traverse = traverse->left;
					continue;
				}
			}
		}
	}
	return 0;
}
#endif /* NO_TRAVCHECK */
