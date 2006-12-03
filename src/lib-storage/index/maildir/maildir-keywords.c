/* Copyright (C) 2005 Timo Sirainen */

/* note that everything here depends on uidlist file being locked the whole
   time. that's why we don't have any locking of our own, or that we do things
   that would be racy otherwise. */

#include "lib.h"
#include "array.h"
#include "ioloop.h"
#include "hash.h"
#include "str.h"
#include "istream.h"
#include "file-dotlock.h"
#include "write-full.h"
#include "maildir-storage.h"
#include "maildir-uidlist.h"
#include "maildir-keywords.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <utime.h>

struct maildir_keywords {
	struct maildir_mailbox *mbox;
	char *path;

	pool_t pool;
	ARRAY_TYPE(keywords) list;
	struct hash_table *hash; /* name -> idx+1 */

        struct dotlock_settings dotlock_settings;

	time_t synced_mtime;
	unsigned int synced:1;
	unsigned int changed:1;
};

struct maildir_keywords_sync_ctx {
	struct maildir_keywords *mk;
	struct mail_index *index;

	const ARRAY_TYPE(keywords) *keywords;
	ARRAY_DEFINE(idx_to_chr, char);
	unsigned int chridx_to_idx[MAILDIR_MAX_KEYWORDS];
};

struct maildir_keywords *maildir_keywords_init(struct maildir_mailbox *mbox)
{
	struct maildir_keywords *mk;

	mk = i_new(struct maildir_keywords, 1);
	mk->mbox = mbox;
	mk->path = i_strconcat(mbox->control_dir,
			       "/" MAILDIR_KEYWORDS_NAME, NULL);
	mk->pool = pool_alloconly_create("maildir keywords", 256);
	i_array_init(&mk->list, MAILDIR_MAX_KEYWORDS);
	mk->hash = hash_create(default_pool, mk->pool, 0,
			       strcase_hash, (hash_cmp_callback_t *)strcasecmp);
	return mk;
}

void maildir_keywords_deinit(struct maildir_keywords *mk)
{
	hash_destroy(mk->hash);
	array_free(&mk->list);
	pool_unref(mk->pool);
	i_free(mk->path);
	i_free(mk);
}

static void maildir_keywords_clear(struct maildir_keywords *mk)
{
	array_clear(&mk->list);
	hash_clear(mk->hash, FALSE);
	p_clear(mk->pool);
}

static int maildir_keywords_sync(struct maildir_keywords *mk)
{
	struct istream *input;
	struct stat st;
	char *line, *p, *new_name;
	const char **strp;
	int fd, idx;

        /* Remember that we rely on uidlist file locking in here. That's why
           we rely on stat()'s timestamp and don't bother handling ESTALE
           errors. */

	if (stat(mk->path, &st) < 0) {
		if (errno == ENOENT) {
			maildir_keywords_clear(mk);
			mk->synced = TRUE;
			return 0;
		}
                mail_storage_set_critical(STORAGE(mk->mbox->storage),
					  "stat(%s) failed: %m", mk->path);
		return -1;
	}

	if (st.st_mtime == mk->synced_mtime) {
		/* hasn't changed */
		mk->synced = TRUE;
		return 0;
	}
	mk->synced_mtime = st.st_mtime;

	fd = open(mk->path, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT) {
			maildir_keywords_clear(mk);
			mk->synced = TRUE;
			return 0;
		}
                mail_storage_set_critical(STORAGE(mk->mbox->storage),
					  "open(%s) failed: %m", mk->path);
		return -1;
	}

	maildir_keywords_clear(mk);
	input = i_stream_create_file(fd, default_pool, 1024, FALSE);
	while ((line = i_stream_read_next_line(input)) != NULL) {
		p = strchr(line, ' ');
		if (p == NULL) {
			/* note that when converting .customflags file this
			   case happens in the first line. */
			continue;
		}
		*p++ = '\0';

		idx = atoi(line);
		if (idx < 0 || idx >= MAILDIR_MAX_KEYWORDS) {
			/* shouldn't happen */
			continue;
		}

		/* save it */
		new_name = p_strdup(mk->pool, p);
		hash_insert(mk->hash, new_name, POINTER_CAST(idx + 1));

		strp = array_idx_modifiable(&mk->list, idx);
		*strp = new_name;
	}
	i_stream_destroy(&input);

	if (close(fd) < 0) {
                mail_storage_set_critical(STORAGE(mk->mbox->storage),
					  "close(%s) failed: %m", mk->path);
		return -1;
	}

	mk->synced = TRUE;
	return 0;
}

static int
maildir_keywords_lookup(struct maildir_keywords *mk, const char *name)
{
	void *p;

	i_assert(maildir_uidlist_is_locked(mk->mbox->uidlist));

	p = hash_lookup(mk->hash, name);
	if (p == NULL) {
		if (mk->synced)
			return -1;

		if (maildir_keywords_sync(mk) < 0)
			return -1;

		p = hash_lookup(mk->hash, name);
		if (p == NULL)
			return -1;
	}

	return POINTER_CAST_TO(p, int)-1;
}

static void
maildir_keywords_create(struct maildir_keywords *mk, const char *name,
			unsigned int chridx)
{
	const char **strp;
	char *new_name;

	i_assert(chridx < MAILDIR_MAX_KEYWORDS);

	new_name = p_strdup(mk->pool, name);
	hash_insert(mk->hash, new_name, POINTER_CAST(chridx + 1));

	strp = array_idx_modifiable(&mk->list, chridx);
	*strp = new_name;

	mk->changed = TRUE;
}

static int
maildir_keywords_lookup_or_create(struct maildir_keywords *mk, const char *name)
{
	const char *const *keywords;
	unsigned int i, count;
	int idx;

	idx = maildir_keywords_lookup(mk, name);
	if (idx >= 0)
		return idx;
	if (!mk->synced) {
		/* we couldn't open the dovecot-keywords file. */
		return -1;
	}

	/* see if we are full */
	keywords = array_get(&mk->list, &count);
	for (i = 0; i < count; i++) {
		if (keywords[i] == NULL)
			break;
	}

	if (i == count && count >= MAILDIR_MAX_KEYWORDS)
		return -1;

        maildir_keywords_create(mk, name, i);
	return i;
}

static const char *
maildir_keywords_idx(struct maildir_keywords *mk, unsigned int idx)
{
	const char *const *keywords;
	unsigned int count;

	i_assert(maildir_uidlist_is_locked(mk->mbox->uidlist));

	keywords = array_get(&mk->list, &count);
	if (idx >= count) {
		if (mk->synced)
			return NULL;

		if (maildir_keywords_sync(mk) < 0)
			return NULL;

		keywords = array_get(&mk->list, &count);
	}
	return idx >= count ? NULL : keywords[idx];
}

static int maildir_keywords_commit(struct maildir_keywords *mk)
{
	struct dotlock *dotlock;
	const char *lock_path, *const *keywords;
	unsigned int i, count;
	struct utimbuf ut;
	string_t *str;
	mode_t old_mask;
	int fd;

	mk->synced = FALSE;

	if (!mk->changed)
		return 0;

	/* we could just create the temp file directly, but doing it this
	   ways avoids potential problems with overwriting contents in
	   malicious symlinks */
	t_push();
	lock_path = t_strconcat(mk->path, ".lock", NULL);
	(void)unlink(lock_path);
        old_mask = umask(0777 & ~mk->mbox->mail_create_mode);
	fd = file_dotlock_open(&mk->dotlock_settings, mk->path,
			       DOTLOCK_CREATE_FLAG_NONBLOCK, &dotlock);
	umask(old_mask);
	if (fd == -1) {
		mail_storage_set_critical(STORAGE(mk->mbox->storage),
			"file_dotlock_open(%s) failed: %m", mk->path);
		t_pop();
		return -1;
	}

	str = t_str_new(256);
	keywords = array_get(&mk->list, &count);
	for (i = 0; i < count; i++) {
		if (keywords[i] != NULL)
			str_printfa(str, "%u %s\n", i, keywords[i]);
	}
	if (write_full(fd, str_data(str), str_len(str)) < 0) {
		mail_storage_set_critical(STORAGE(mk->mbox->storage),
			"write_full(%s) failed: %m", mk->path);
		(void)file_dotlock_delete(&dotlock);
		t_pop();
		return -1;
	}

	/* mtime must grow every time */
        mk->synced_mtime = ioloop_time <= mk->synced_mtime ?
		mk->synced_mtime + 1 : ioloop_time;
	ut.actime = ioloop_time;
	ut.modtime = mk->synced_mtime;
	if (utime(lock_path, &ut) < 0) {
		mail_storage_set_critical(STORAGE(mk->mbox->storage),
			"utime(%s) failed: %m", lock_path);
		return -1;
	}

	if (file_dotlock_replace(&dotlock, 0) < 0) {
		mail_storage_set_critical(STORAGE(mk->mbox->storage),
			"file_dotlock_replace(%s) failed: %m", mk->path);
		t_pop();
		return -1;
	}

	mk->changed = FALSE;
	t_pop();
	return 0;
}

struct maildir_keywords_sync_ctx *
maildir_keywords_sync_init(struct maildir_keywords *mk,
			   struct mail_index *index)
{
	struct maildir_keywords_sync_ctx *ctx;

	ctx = i_new(struct maildir_keywords_sync_ctx, 1);
	ctx->mk = mk;
	ctx->index = index;
	ctx->keywords = mail_index_get_keywords(index);
	i_array_init(&ctx->idx_to_chr, MAILDIR_MAX_KEYWORDS);
	return ctx;
}

void maildir_keywords_sync_deinit(struct maildir_keywords_sync_ctx *ctx)
{
	maildir_keywords_commit(ctx->mk);
	array_free(&ctx->idx_to_chr);
	i_free(ctx);
}

unsigned int maildir_keywords_char_idx(struct maildir_keywords_sync_ctx *ctx,
				       char keyword)
{
	const char *name;
	unsigned int chridx, idx;

	i_assert(keyword >= MAILDIR_KEYWORD_FIRST &&
		 keyword <= MAILDIR_KEYWORD_LAST);
	chridx = keyword - MAILDIR_KEYWORD_FIRST;

	if (ctx->chridx_to_idx[chridx] != 0)
		return ctx->chridx_to_idx[chridx];

	/* lookup / create */
	name = maildir_keywords_idx(ctx->mk, chridx);
	if (name == NULL) {
		/* name is lost. just generate one ourself. */
		name = t_strdup_printf("unknown-%u", chridx);
		while (maildir_keywords_lookup(ctx->mk, name) >= 0) {
			/* don't create a duplicate name.
			   keep changing the name until it doesn't exist */
			name = t_strconcat(name, "?", NULL);
		}
                maildir_keywords_create(ctx->mk, name, chridx);
	}

	if (!mail_index_keyword_lookup(ctx->index, name, TRUE, &idx))
		i_unreached();

        ctx->chridx_to_idx[chridx] = idx;
	return idx;
}

char maildir_keywords_idx_char(struct maildir_keywords_sync_ctx *ctx,
			       unsigned int idx)
{
	const char *const *name_p;
	char *chr_p;
	int chridx;

	chr_p = array_idx_modifiable(&ctx->idx_to_chr, idx);
	if (*chr_p != '\0')
		return *chr_p;

	name_p = array_idx(ctx->keywords, idx);
	chridx = maildir_keywords_lookup_or_create(ctx->mk, *name_p);
	if (chridx < 0)
		return '\0';

	*chr_p = chridx + MAILDIR_KEYWORD_FIRST;
	return *chr_p;
}
