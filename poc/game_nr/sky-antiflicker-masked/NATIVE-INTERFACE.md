# Native masked interface (canonical4667d5f8)

## Selector / no off allocation

`sky_antiflicker::selector(nullptr)` and `selector("off")` returnoff; exactlow_temporal/regional_antipop select candidates1/2. Unknown strings reject. Off retains old e99,21bindings, and does NOT instantiate `SkyAntiflickerMaskedOwned`. Its constructor rejectsoff before resource allocation. Host tuple/history/epoch/gap/reset/oldest-contributing-origin age<=4 and actual priorSR success still gate history. NR Reset1 is not a cache reset.

## GPU layout

CSMain8x8. Old0..20 bindings and CB0..152 unchanged. Binding21 ContextNow109320x180,22 ContextPrev109320x180;23 readonly STORAGE_BUFFER exactly4B, offset0. Pipeline24bindings:1uniform,14sampled,7storageimages,1sampler,1storagebuffer. Storage formatsRGBA32F109, statsR32_UINT98. CB256: WholeValiduint156/AntiPopStepfloat160/Tolfloat164=.15/CtxWuint168/CtxHuint172. Mask bit0(1) admits previousShown/previousShownGuide; bit1(2) additionally admits previousContext. Bit1 alone cannot admit any history. Only existingMode10 predicates change; previous image/context reads stay inside those branches. CurrentContext can be generated regardless of previous numeric validity.

## Native return handoff (B owns implementation)

TwoGPU-only device-local4B buffers, no mapped storage/CPU valid setter. For each supportedcurrent source, includingraw/invalid path, samequeue FillBuffer(next,0,4,0), TRANSFER_WRITE->COMPUTE SHADER_READ|WRITE. Native's existing return pass0 aggregates whole-frame `failed`. After COMPUTE_WRITE->COMPUTE_READ dependency, returnpass1 binding4 writes onlyq=(0,0): `next = failed==0 && wholeEligible ? (1 | (currentContextWritten ? 2 : 0)) : 0`. Never markvalid before this guard, never depend on a CPU candidate-success boolean alone. Returnmask belongs to the same physicalnextShown/context/source/epoch tuple.

After previous returnmask/shown/guide/context writes, require COMPUTE_WRITE->nextCOMPUTE_READ plus existing samequeue scheduler lastuse dependency. Do not readback mask or add renderer wait/Flush. Native uses per-record immutable256BUBO/descriptor sets; no reuse whilepending. Only actual fence/tick proof can retire/reuse them. Present association remains independent; bit0 proves numericallyvalid SRinput, not actual Present.

## Owned recording entry points

`record_initialize_masks()` clearsboth. Submit/complete actualfence, then `commit_shown_after_fence()` before anyhistory/apply. `record_history(prev,next,...)` is unchangedr2Capture2/Reproject1/Pyramid3, except initializedmask prerequisite. Actual safe fence before slot reuse.

`record_apply(bank,currentRaw109,currentDepth100,currentMV103,Admission,candidate,actual_dt,jitter_delta)` first clearsnextmask and supplies previousmask to binding23. Context12 onlycandidate2. Apply4→rawEdit (full-strength). Immediately copy **this final CURRENT Apply bank.guide** to independentnextShownGuide, restoreGENERAL, thenMode10 writesfinal109/nextShown. Never copyorigin/intermediatecatchup or retainmutable bankview. `pending_return_mask()` supplies exactlythat recordednext4B VkBuffer forB returnpass1; no nonzero write inA. Commitmetadata onlyafter returnpass/realSRlastuse inNative (standalone caller uses actualownedfence). Candidate2Step=.75*smoothedrealpositivedt; oldclock .9 + currentclamped[1/500,1/10]*.1. No legalprevcontext meansStep0. Bit1 still required inGPU before readingprevContext.

`record_clear_next_mask(next)` handlesraw/unsupportedcurrent path AFTERrealpriorlastuse. On actualfencecommit it switchesprevious slotto theclearednext and retires CPUgeometry/context; it cannot leave oldvalidslot active. `previous_mask()` returns readonlyhandle+bytes, unmapped; `pending_return_mask()` only whilecurrentApplypending. Native can port these exact records into itsscheduler but must not use standalone commit methods as synthetic schedulerproof.

Recordedbutunproved destructor usesHOLD; unknownGPU nofree. Realcompletion includesmaskclear/read/write, shown/context/guide and privatehalf/SR read. Nohistoryvalid claim basedon CPUguard omission. Allcurrentraw inputs immutable and cannotalias editedoutput/history.

## Limitations

No legalprior across WholeInvalid/restart/gap/ageexpiry. Invalid and zeroedit preservecurrentraw negativeRGB/signedzero/alpha identity according to existingr2 guard. Lowpixelconfidence does not invalidatewholecache: unchangedpyramid filling is preserved. No model/720guide/NRphase or longerhistory claim; no realGPU test inthis package.
