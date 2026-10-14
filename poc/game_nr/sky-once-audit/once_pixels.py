#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only byte/math audit core. B's actual cold metadata adapter is intentionally pending.
No transport schema, synthetic GPU proof, model, renderer or runtime invocation.
"""
from pathlib import Path
import hashlib
import json
import numpy as np

WIDTH, HEIGHT = 1280, 720
F32_U = 2.0**-23  # conservative full ULP relative bound, covers directed rounding as well as RTE
LUMA = np.array([.2126, .7152, .0722], dtype='<f4').astype(np.float64)
EPS = 1.0/512.0
LOG2_NEAR_ONE_ABS = 2.0**-21
PRECISION_URL = 'https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#precision-of-glsl-std-450-instructions'
FROZEN_SHADER = Path('/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-antiflicker-masked-worktree/poc/game_nr/sky-antiflicker-masked/out/sky_antiflicker.hlsl')
FROZEN_SPV_SHA = '3e31ea305d15bab0ff7e2a657bc952e768027d8b77087d2b21abe22993936270'

def finite_scalar(x):
    x=float(x)
    return x if np.isfinite(x) else None

def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()

def exact_array(data, dtype, shape):
    expected = int(np.prod(shape))*np.dtype(dtype).itemsize
    if len(data) != expected:
        raise ValueError(f'exact byte count {len(data)} != {expected}, {dtype} {shape}')
    return np.frombuffer(data, dtype=dtype).reshape(shape)

def ulp32_envelope(magnitude):
    # Magnitudes only: never np.spacing(negative), and never a residual-derived budget.
    x = np.abs(np.asarray(magnitude, dtype=np.float64)).astype(np.float32)
    return np.nextafter(x, np.float32(np.inf)).astype(np.float64)-x.astype(np.float64)

def block_mean(a):
    h, w = a.shape
    if h % 4 or w % 4:
        raise ValueError('initial exact full4x4 Context12 blocks only')
    return a.reshape(h//4,4,w//4,4).mean(axis=(1,3), dtype=np.float64)

def context12_reference(raw32):
    """WP1 only; full float32 shader PrintOf.x and immutable input-only error envelope.
    Reference is the ideal expression with the ACTUAL float32 kLuma constants.
    Budget covers p=max(c,0)+eps then p-eps, dot3, eps add, log2 (Vulkan rule),
    log2(white1), subtraction, <=15 sum additions, final division/store.
    Reassociation/FMA may lower error; no relaxation based on observed GPU output.
    """
    if raw32.dtype != np.dtype('<f4') or raw32.ndim != 3 or raw32.shape[2] != 4:
        raise ValueError('RGBA32F source')
    if not np.isfinite(raw32).all():
        raise ValueError('finite source required before Context arithmetic')
    c = np.maximum(raw32[...,:3].astype(np.float64), 0.)
    ideal_dot = (c*LUMA).sum(axis=2)
    ideal_x = ideal_dot + EPS
    # Both separate add/sub and compiler cancellation are admitted by this fixed envelope.
    add_err = F32_U*(c+EPS)
    sub_err = add_err + F32_U*(c+add_err)
    input_err = (sub_err*LUMA).sum(axis=2)
    gamma5 = 5*F32_U/(1-5*F32_U)  # three products + two additions in dot3
    dot_err = input_err + gamma5*((c+sub_err)*LUMA).sum(axis=2)
    x_err = dot_err + F32_U*(ideal_dot+dot_err+EPS)
    lo, hi = ideal_x-x_err, ideal_x+x_err
    if np.any(lo <= 0):
        raise ValueError('positive log argument bound required')
    ideal_log = np.log2(ideal_x)
    propagated = np.maximum(np.log2(hi)-ideal_log, ideal_log-np.log2(lo))
    log_ulp = np.maximum(ulp32_envelope(np.log2(lo)), ulp32_envelope(np.log2(hi)))
    # Always cover near-one absolute rule as well as outside-range 3 ULP. Fixed before data arrival.
    log_error = np.maximum(3*log_ulp, LOG2_NEAR_ONE_ABS)
    white_log_error = LOG2_NEAR_ONE_ABS  # actual shader log2(WhitePoint1), NOT assumed exact/optimized
    print_error = propagated + log_error + white_log_error
    print_error += F32_U*(np.abs(ideal_log)+print_error)
    gamma15 = 15*F32_U/(1-15*F32_U)
    expected = block_mean(ideal_log)
    bound = block_mean(print_error) + gamma15*block_mean(np.abs(ideal_log)+print_error)
    # Division by16 is normally exact binary scaling. Still cover generic OpFDiv/store rounding.
    bound += 3*ulp32_envelope(np.abs(expected)+bound)
    full = np.zeros((*expected.shape,4), dtype='<f4'); full[...,0] = expected.astype('<f4')
    return full, expected, bound

def compare_context(actual, raw32):
    _, expected, bound = context12_reference(raw32)
    if actual.shape != (*expected.shape,4) or actual.dtype != np.dtype('<f4'):
        raise ValueError('actual Context12 exact quarter RGBA32F extent')
    finite = np.isfinite(actual)
    residual = np.abs(actual[...,0].astype(np.float64)-expected)
    fail = (~finite[...,0]) | (residual > bound)
    yz_bad = np.any(actual[...,1:].view('<u4') != 0, axis=2)  # literal float4(x,+0,+0,+0)
    coords = np.argwhere(fail | yz_bad)
    samples = []
    for y,x in coords[:8]:
        samples.append({'x':int(x),'y':int(y),'actual':finite_scalar(actual[y,x,0]),
                        'expected_ideal':float(expected[y,x]),'absolute_error':finite_scalar(residual[y,x]),
                        'predeclared_bound':float(bound[y,x]),'actual_bits':[int(v) for v in actual[y,x].view('<u4')]})
    return {'status':'PASS' if not len(coords) else 'FAIL','finite_channels':int(finite.sum()),
            'total_channels':int(actual.size),'mismatched_blocks':int(len(coords)),
            'max_abs_error':finite_scalar(np.max(residual)), 'max_predeclared_bound':float(bound.max()),
            'min_predeclared_bound':float(bound.min()),'first_mismatches':samples,
            'rule':'WP1_PrintOf_float32_dot3_log2_16accumulate_input_only_bound_v1',
            'precision_source':PRECISION_URL,'bound_fitted_to_residual':False,
            'GPU_source_identity':'REQUIRES_B_ACTUAL_METADATA_AND_ROOT_COMPLETION_JOIN'}

def audit_bytes(*, raw97, sr_input97, guard4, masks8, shown_guide32, final_guide32,
                current_context32, selector, whole_eligible, context_written):
    """File payload core only; host flags describe expected formula, never establish GPU proof.
    No metadata aliases chosen here. Mapping B's final fields/filenames is a later narrow step.
    """
    if selector not in ('low_temporal','regional_antipop'):
        raise ValueError('masked candidate selector only')
    if type(whole_eligible) is not bool or type(context_written) is not bool:
        raise ValueError('strict bool host declarations')
    if context_written and selector != 'regional_antipop':
        raise ValueError('Context12 only regional_antipop')
    raw = exact_array(raw97,'<f2',(HEIGHT,WIDTH,4))
    actual = exact_array(sr_input97,'<f2',(HEIGHT,WIDTH,4))
    flag = int(exact_array(guard4,'<u4',(1,))[0])
    prev, nxt = (int(v) for v in exact_array(masks8,'<u4',(2,)))  # internal argument order; NOT wire schema
    shown = exact_array(shown_guide32,'<f4',(HEIGHT,WIDTH,4))
    guide = exact_array(final_guide32,'<f4',(HEIGHT,WIDTH,4))
    ctx = exact_array(current_context32,'<f4',(HEIGHT//4,WIDTH//4,4)) if current_context32 is not None else None
    if context_written and ctx is None:
        raise ValueError('written Context12 payload required; no fabricated unwritten context')
    checks=[]
    def check(name,ok): checks.append({'name':name,'status':'PASS' if bool(ok) else 'FAIL'})
    check('original_and_actual_all_channels_finite',np.isfinite(raw).all() and np.isfinite(actual).all())
    alpha = raw[...,3].view('<u2') == actual[...,3].view('<u2')
    check('alpha_original_half_bits',alpha.all())
    check('guard_atomic_or_domain',flag in (0,1))
    check('mask_previous_valid_domain',prev in (0,1,3))
    expected_next=(1|(2 if context_written else 0)) if flag==0 and whole_eligible else 0
    check('mask_next_matches_actual_guard_and_host_scope',nxt==expected_next)
    check('independent_CURRENT_guide_copy_bit_exact',shown_guide32==final_guide32)
    check('CURRENT_guide_pair_all_finite',np.isfinite(shown).all() and np.isfinite(guide).all())
    raw_fallback = 'NOT_TRIGGERED'
    if flag:
        check('guard_failure_entire_actual_input_original_bits',sr_input97==raw97)
        raw_fallback='PASS' if sr_input97==raw97 else 'FAIL'
    delta = actual[...,:3].astype(np.float64)-raw[...,:3].astype(np.float64)
    rgb_bits_changed = actual[...,:3].view('<u2') != raw[...,:3].view('<u2')
    context_result={'status':'NOT_APPLICABLE_CONTEXT_NOT_RECORDED','contents_not_called_valid':True}
    if context_written:
        if np.isfinite(raw).all():
            context_result=compare_context(ctx,raw.astype('<f4'))
            check('CURRENT_Context12_original_raw_oracle',context_result['status']=='PASS')
        else:
            context_result={'status':'FAIL_NONFINITE_SOURCE'};check('CURRENT_Context12_original_raw_oracle',False)
    energy={'RGB_difference_RMS':finite_scalar(np.sqrt(np.mean(delta*delta))),
            'RGB_difference_mean_abs':finite_scalar(np.mean(np.abs(delta))),
            'RGB_difference_max_abs':finite_scalar(np.max(np.abs(delta))),
            'changed_RGB_channels_bits':int(rgb_bits_changed.sum()),
            'changed_RGB_pixels_bits':int(np.any(rgb_bits_changed,axis=2).sum()),
            'meaning':'actual shown SR-input minus same-current raw, not immutable-origin NR inference truth',
            'nonzero_edit_acceptance':'OBSERVED' if np.any(rgb_bits_changed) else 'NO_OBSERVED_EDIT',
            'nonzero_edit_is_not_required_for_pixel_copy_validity':True}
    return {'status':'PASS_PIXELS_ONLY' if all(c['status']=='PASS' for c in checks) else 'FAIL_PIXELS',
            'checks':checks,'alpha_equal_pixels':int(alpha.sum()),'pixel_count':WIDTH*HEIGHT,
            'guard_actual':flag,'previous_mask_actual':prev,'next_mask_actual':nxt,
            'next_mask_formula_expected':expected_next,'raw_guard_fallback':raw_fallback,
            'context':context_result,'edit_energy':energy,
            'payload_sha256':{n:(sha_bytes(v) if v is not None else None) for n,v in [('raw97',raw97),('sr_input97',sr_input97),('guard4',guard4),('masks8',masks8),('shown_guide32',shown_guide32),('final_guide32',final_guide32),('current_context32',current_context32)]},
            'scope':'ONLY_ELIGIBLE_ONCE_PAYLOADS; raw/WholeInvalid/partial-record branch proofs NOT_MEASURED',
            'identity_GPU_completion_Present_retirement':'NOT_CHECKED_BY_PAYLOAD_CORE',
            'previous_mask_gate_execution':'NOT_PROVED_BY_A_SINGLE_PREV_NEXT_WORD_PAIR',
            'NR_output_phase_quality_performance':'UNVERIFIED',
            'metadata_adapter':'audit_once.py maps exact frozen e02b6bc producer; payload core alone does not inspect metadata'}
