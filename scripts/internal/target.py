"""Select a maintained native producer target from the shared build contract."""
import platform


def host_target(targets, system=None, machine=None):
    system = system or platform.system()
    machine = (machine or platform.machine()).lower()
    architecture = {'amd64': 'x86_64', 'aarch64': 'arm64'}.get(machine, machine)
    for name, target in targets.items():
        if target['host'].lower() == system.lower() and target['architecture'] == architecture:
            return name, target
    raise RuntimeError('Unsupported native host/target: '+system+' '+machine)
