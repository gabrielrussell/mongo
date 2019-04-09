"""Extension to the logging package to support buildlogger."""

# Alias the built-in logging.Logger class for type checking arguments. Those interested in
# constructing a new Logger instance should use the loggers.new_logger() function instead.
from logging import Logger

from . import buildlogger
from . import flush
from . import loggers
