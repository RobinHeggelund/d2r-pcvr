"""Print current subscription status, using only the plugin's shared mapping."""
import json
from pathlib import Path
from foliage_runtime_status import read_status

INI = Path(r'E:\Games\Diablo II Resurrected\mods\pcvr-dev\d2rloader\plugins\d2r_pcvr.ini')
if __name__ == '__main__':
    status = read_status(INI)
    if 'subscription_requested_rings' not in status:
        raise SystemExit('The running DLL does not include the subscription experiment yet.')
    print(json.dumps({key: value for key, value in status.items()
                      if key in ('pid', 'in_world') or key.startswith('subscription_')}, indent=2))
