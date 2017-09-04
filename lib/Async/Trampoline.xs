#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "Scheduler.h"
#include "Async.h"

static
void*
copy_sv_ref(void* sv)
{
    SvREFCNT_inc((SV*) sv);
    return sv;
}

static
void
destroy_sv_ref(void* sv)
{
    SvREFCNT_dec((SV*) sv);
}

static Destructible_Vtable sv_vtable = {
    .copy       = copy_sv_ref,
    .destroy    = destroy_sv_ref,
};

static
Async*
invoke_cv(Destructible* body_container, DestructibleTuple* args)
{
    CV* callback = (CV*) body_container->data;
    assert(callback);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        SV* name_sv = cv_name(callback, NULL, 0);
        const char* name = SvPV_nolen(name_sv);
        const char* file = CvFILE(callback);
        const COP* location = (const COP*) CvSTART(callback);
        int line = (location) ? CopLINE(location) : 0;

        ASYNC_LOG_DEBUG(
                "running Perl callback %p: %s() at %s line %d",
                callback, name, file, line);
    }

    dSP;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);
    if (args && args->size)
    {
        EXTEND(SP, args->size);
        for (size_t i = 0; i < args->size; i++)
            PUSHs((SV*) DestructibleTuple_at(args, i));
        PUTBACK;
    }

    // TODO handle exceptions
    int count = call_sv((SV*) callback, G_SCALAR);

    SPAGAIN;

    assert(count == 1);  // safe because scalar context

    SV* result_sv = POPs;

    if (!sv_isa(result_sv, "Async::Trampoline"))
        croak("Async callback must return another Async!");

    Async* result = (Async*) SvIV(SvRV(result_sv));
    Async_ref(result);  // take ownership because result_sv will be destroyed

    FREETMPS;
    LEAVE;

    return result;
}

MODULE = Async::Trampoline PACKAGE = Async::Trampoline

void
run_until_completion(async)
        Async* async;
    PROTOTYPE: $;
    PPCODE:
    {
        Async_run_until_completion(async);

        async = Async_Ptr_follow(async);

        if (async->type < Async_CATEGORY_COMPLETE)
            croak(  "run_until_completion() did not complete the Async "
                    "(type code %d)",
                    async->type);

        if (async->type == Async_IS_CANCEL)
        {
            croak("run_until_completion(): Async was cancelled");
        }
        else if (async->type == Async_IS_ERROR)
        {
            croak_sv((SV*) async->as_error.data);
        }
        else if (async->type == Async_IS_VALUE)
        {
            DestructibleTuple* values = async->as_value;
            for (size_t i = 0; i < values->size; i++) {
                SV* orig_value = (SV*) DestructibleTuple_at(values, i);
                XPUSHs(sv_2mortal(newSVsv(orig_value)));
            }
        }
        else
        {
            assert(0);
        }
    }

void
DESTROY(async)
        Async* async;
    CODE:
        Async_unref(async);

Async*
async(body)
        CV* body
    PROTOTYPE: &
    CODE:
    {
        Async* self = Async_new();
        Async_Thunk_init(
                self,
                invoke_cv,
                MAKE_DESTRUCTIBLE(&sv_vtable, body),
                NULL);
        SvREFCNT_inc(body);
        RETVAL = self;
    }
    OUTPUT: RETVAL

Async*
await(dep, body)
        Async*  dep;
        CV*     body;
    PROTOTYPE: $&
    CODE:
    {
        Async* self = Async_new();
        Async_Thunk_init(
                self,
                invoke_cv,
                MAKE_DESTRUCTIBLE(&sv_vtable, body),
                dep);
        SvREFCNT_inc(body);
        RETVAL = self;
    }
    OUTPUT: RETVAL

Async*
async_value(...)
    PROTOTYPE: @
    CODE:
    {
        DestructibleTuple* values = DestructibleTuple_new(&sv_vtable, items);
        for (size_t i = 0; i < items; i++)
        {
            Destructible destructible_value = {
                .vtable = &sv_vtable,
                .data   = newSVsv(ST(i)),
            };
            DestructibleTuple_move_into(values, i, &destructible_value);
        }

        Async* self = Async_new();
        Async_Value_init(self, values);
        RETVAL = self;
    }
    OUTPUT: RETVAL

Async*
async_cancel()
    PROTOTYPE:
    CODE:
    {
        Async* self = Async_new();
        Async_Cancel_init(self);
        RETVAL = self;
    }
    OUTPUT: RETVAL

Async*
async_resolved_or(first, orelse)
        Async*  first;
        Async*  orelse;
    PROTOTYPE: DISABLE
    CODE:
    {
        Async* self = Async_new();
        Async_ResolvedOr_init(self, first, orelse);
        RETVAL = self;
    }
    OUTPUT: RETVAL
