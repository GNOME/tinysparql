import os


def get_environment_boolean(variable):
    '''Parse a yes/no boolean passed through the environment.'''

    value = os.environ.get(variable, 'no').lower()
    if value in ['no', '0', 'false']:
        return False
    elif value in ['yes', '1', 'true']:
        return True
    else:
        raise RuntimeError('Unexpected value for %s: %s' %
                           (variable, value))


def is_verbose():
    """
    True to log process status information to stdout
    """
    return get_environment_boolean('TRACKER_TESTS_VERBOSE')


def is_manual_start():
    """
    False to start the processes automatically
    """
    return get_environment_boolean('TRACKER_TESTS_MANUAL_START')
