#!/usr/bin/env python3
"""Product 89 browser-builder structural and binary-parity tests."""
from pathlib import Path
import hashlib, json, re, subprocess, tempfile
import sys

ROOT=Path(__file__).resolve().parents[1]
OTA=(ROOT/'src'/'ota_update.cpp').read_text(encoding='utf-8')
BUILD=(ROOT/'include'/'build_info.h').read_text(encoding='utf-8')

def req(s,n): assert n in s, f'missing Product 89 requirement: {n}'

def main():
    req(BUILD,'FIRMWARE_VERSION_CODE = 89')
    req(BUILD,'PRODUCT89-BROWSER-AIRPORT-BUILDER')
    for n in ['BUILD &amp; INSTALL AIRPORT DATABASE','AIRPORTS_URL','RUNWAYS_URL','parseAirports(','applyRunways(','buildRadarapt(','sha256(','mode:\'cors\'','MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT','server.on("/airports/upload"']:
        req(OTA,n)
    req(OTA,'Advanced: install an existing .radarapt file')
    assert 'navigator.geolocation' not in OTA
    assert 'crypto.subtle' not in OTA
    # Center coordinates must never be passed to buildRadarapt/header metadata.
    call=re.search(r'buildRadarapt\(parsed\.airports,today,coverage,radius\)',OTA)
    assert call

    m=re.search(r'// AIRPORT_BROWSER_BUILDER_BEGIN\n(.*?)// AIRPORT_BROWSER_BUILDER_END',OTA,re.S)
    assert m, 'browser builder marker block missing'
    js=m.group(1)
    synthetic=[{
      'ident':'KAAA','name':'ALPHA FIELD','latitude':42.5,'longitude':-88.25,
      'elevation':777,'runway_length':5400,'runway_heading':90,'category':1,'internal':'KAAA'
    },{
      'ident':'PVT1','name':'PRIVATE TEST','latitude':42.75,'longitude':-88.5,
      'elevation':-5,'runway_length':1800,'runway_heading':180,'category':2,'internal':'PVT1'
    }]
    node=js+"\nconst x=buildRadarapt("+json.dumps(synthetic)+",'2026-08-13','TEST REGION',120); process.stdout.write(Buffer.from(x).toString('hex'));\n"
    with tempfile.NamedTemporaryFile('w',suffix='.js',delete=False,encoding='utf-8') as f:
        f.write(node); path=f.name
    result=subprocess.run(['node',path],capture_output=True,text=True,check=True)
    got=bytes.fromhex(result.stdout.strip())
    sys.path.insert(0,str(ROOT/'tools'))
    from airport_package import build_package
    class A:
        def __init__(self,d): self.__dict__.update(d)
    expected=build_package([A(x) for x in synthetic],database_date='2026-08-13',coverage='TEST REGION',radius_miles=120,generator_version=3)
    assert got==expected, f'browser package differs from Python format: {hashlib.sha256(got).hexdigest()} != {hashlib.sha256(expected).hexdigest()}'
    print('Product 89 browser airport builder checks passed')
if __name__=='__main__': main()
