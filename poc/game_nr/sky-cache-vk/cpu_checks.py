#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import copy,json,re
from pathlib import Path
from fixture_schema import validate,unique

def checks(cases,assembly):
 rows=[]
 for p in cases:
  c=json.loads(Path(p).read_text(),object_pairs_hook=unique);validate(c);rows.append({'check':'case_exact_assets_'+c['case_id'],'status':'PASS'})
 good=json.loads(Path(cases[0]).read_text());mutations=[('schema',lambda c:c.update(schema='other')),('key',lambda c:c.update(extra=1)),('bool_links',lambda c:c.update(max_links=True)),('five_links',lambda c:c.update(max_links=5)),('over_frames',lambda c:c.update(frames=c['frames']*2)),('zero_extent',lambda c:c.update(color_extent=[0,8])),('bool_extent',lambda c:c.update(color_extent=[True,8])),('known_unequal',lambda c:c.update(guide_extent=[8,8])),('wrong_bytes',lambda c:c['frames'][0]['assets']['mv'].update(bytes=4)),('unsigned_depth_format',lambda c:c['frames'][0]['assets']['depth'].update(format=True)),('posix_windows_path',lambda c:c['frames'][0]['assets']['source'].update(windows_path=c['frames'][0]['assets']['source']['path'])),('bad_sha',lambda c:c['frames'][0]['assets']['source'].update(sha256='X'*64)),('wrong_budget',lambda c:c['budgets_ms'].update(fence=10001)),('reset_int',lambda c:c['frames'][0].update(reset=1)),('unbounded_source',lambda c:c['frames'][0].update(source=2**53)),('nan_jitter',lambda c:c['frames'][0].update(jitter_delta_uv=[float('nan'),0])),('refresh_origin_mismatch',lambda c:c['frames'][0].update(origin=c['frames'][0]['source']-1)),('no_anchor',lambda c:c['frames'][0].update(refresh=False)),('false_real_oracle',lambda c:c.update(phase_status='UNVERIFIED_REAL_GUIDE_DOMAIN'))]
 for name,change in mutations:
  c=copy.deepcopy(good);change(c)
  try:validate(c,assets=False)
  except (ValueError,KeyError):pass
  else:raise AssertionError('Negative accepted: '+name)
  rows.append({'check':'schema_reject_'+name,'status':'PASS'})
 try:json.loads('{"schema":1,"schema":2}',object_pairs_hook=unique)
 except ValueError:rows.append({'check':'duplicate_json_reject','status':'PASS'})
 else:raise AssertionError('duplicate allowed')
 a=Path(assembly).read_text();offsets={int(i):int(x) for i,x in re.findall(r'OpMemberDecorate %Params (\d+) Offset (\d+)',a)}
 assert offsets=={i:i*4 for i in range(40)},offsets
 rows.append({'check':'all40_HLSL_scalar_offsets_0_to156','status':'PASS'})
 bindings={name:int(k) for name,k in re.findall(r'OpDecorate %(\w+) Binding (\d+)',a)}
 expected={'_':0,'gHistEdit':1,'gHistGuide':2,'gColour':3,'gDepth':4,'gMotion':5,'gAux0':6,'gAux1':7,'gAux2':8,'gAux3':9,'gExposure':11,'gHistTarget':12,'gOut0':13,'gOut1':14,'gOut2':15,'gOut3':16,'gOut4':17,'gStats':18,'gOut6':19,'gLinear':20}
 assert bindings==expected,(bindings,expected)
 assert 'OpExecutionMode %CSMain LocalSize 8 8 1' in a
 assert re.findall(r'OpCapability (\w+)',a)==['Shader']
 rows.append({'check':'explicit21_layout_20_used_bindings_CSMain8x8_shader_only','status':'PASS'})
 return {'status':'PASS_CPU_ONLY','checks':rows,'checks_count':len(rows),'schema_negative_count':len(mutations)+1,'GPU':'NOT_RUN','Wine':'NOT_RUN','model':'NOT_RUN','phase':'UNVERIFIED_REAL_GUIDE_DOMAIN'}
