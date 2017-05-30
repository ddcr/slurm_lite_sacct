class AllenIntervalRules(object):
    """Assume that x, y are of tuples
    """
    @staticmethod
    def ContainedBy(x, y):
        return (x[0] > y[0]) and (x[1] < y[1])

    @staticmethod
    def Contains(x, y):
        return (y[0] > x[0]) and (y[1] < x[1])

    @staticmethod
    def FinishedBy(x, y):
        return (y[1] == x[1]) and (y[0] > x[0])

    @staticmethod
    def Finishes(x, y):
        return (x[1] == y[1]) and (x[0] > y[0])

    @staticmethod
    def IsEqualTo(x, y):
        return (x[0] == y[0]) and (x[1] == y[1])

    @staticmethod
    def Meets(x, y):
        return (x[1] == y[0])

    @staticmethod
    def MetBy(x, y):
        return (y[1] == x[0])

    @staticmethod
    def OverlapedBy(x, y):
        return (y[0] < x[0]) and ((y[1] > x[0]) and (y[1] < x[1]))

    @staticmethod
    def Overlaps(x, y):
        return (x[0] < y[0]) and ((x[1] > y[0]) and (x[1] < y[1]))

    @staticmethod
    def StartedBy(x, y):
        return (y[0] == x[0]) and (y[1] < x[1])

    @staticmethod
    def Starts(x, y):
        return (x[0] == y[0]) and (x[1] < y[1])

    @staticmethod
    def TakesPlaceAfter(x, y):
        return x[0] > y[1]

    @staticmethod
    def TakesPlaceBefore(x, y):
        return x[1] < y[0]
