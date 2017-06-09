class AllenIntervalRules(object):
    """Temporal rules: evaluate temporal relations between
        time intervals. Based on Allen's Interval Algebra:
        https://www.ics.uci.edu/~alspaugh/cls/shr/allen.html.
        Intervals x and y are tuples: (x_tstart, x_tend)
    """
    @staticmethod
    def containedby(x, y):
        return (x[0] > y[0]) and (x[1] < y[1])

    @staticmethod
    def contains(x, y):
        return (y[0] > x[0]) and (y[1] < x[1])

    @staticmethod
    def finishedby(x, y):
        return (y[1] == x[1]) and (y[0] > x[0])

    @staticmethod
    def finishes(x, y):
        return (x[1] == y[1]) and (x[0] > y[0])

    @staticmethod
    def isequalto(x, y):
        return (x[0] == y[0]) and (x[1] == y[1])

    @staticmethod
    def meets(x, y):
        return (x[1] == y[0])

    @staticmethod
    def metby(x, y):
        return (y[1] == x[0])

    @staticmethod
    def overlapedby(x, y):
        return (y[0] < x[0]) and ((y[1] > x[0]) and (y[1] < x[1]))

    @staticmethod
    def overlaps(x, y):
        return (x[0] < y[0]) and ((x[1] > y[0]) and (x[1] < y[1]))

    @staticmethod
    def startedby(x, y):
        return (y[0] == x[0]) and (y[1] < x[1])

    @staticmethod
    def starts(x, y):
        return (x[0] == y[0]) and (x[1] < y[1])

    @staticmethod
    def takesplaceafter(x, y):
        """preceded by"""
        return x[0] > y[1]

    @staticmethod
    def takesplacebefore(x, y):
        """preceds"""
        return x[1] < y[0]
