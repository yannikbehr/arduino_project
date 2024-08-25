#
# author: Jonas Behr 
# date: Sat Aug 24 15:37:02 2024
######################################

import plotly.express as px
import pandas as pd
import numpy as np
from numpy.polynomial import Chebyshev
from tmp_dat import tmp
from numpy.polynomial.polynomial import polyfit


df = pd.DataFrame([{"tmp1": p[0], "tmp2": p[1]} for p in tmp])
df.loc[0:490, "target"] = 22
df.loc[825:1390, "target"] = 2
df.loc[1748:, "target"] = 48

#px.scatter(df, y=["tmp1", "tmp2"]).show()

df = df[df["target"] > 0]

tmp_calcs = {}
for col in ["tmp1", "tmp2"]:
    tmp_calcs[col] = {
            "median":{}, 
            "std":{}, 
            }
    #z = Chebyshev.fit(df[col], df["target"], deg=1).coef
    z = polyfit(df[col], df["target"], deg=2)
    print(f"{col}: {z}")
    #z = np.polyfit(df[col], df["target"], 3)
    tmp_calcs[col]["poly_fit"] = z
    for tmp in [2, 22, 48]:
        tmp_calcs[col]["median"][tmp] = np.median(df[df['target']==tmp][col])
        tmp_calcs[col]["std"][tmp] = np.std(df[df['target']==tmp][col])
        print(f"Temp sensor {col} at temp {tmp}: mean: {tmp_calcs[col]['median']} std: {tmp_calcs[col]['std']}")

for col in ["tmp1", "tmp2"]:
    #coef = (48 - 2) / (tmp_calcs[col]["median"][48] - tmp_calcs[col]["median"][2])
    #offset = tmp_calcs[col]["median"][2] - 2 * coef
    #tmp_calcs[col]["coef"] = coef
    #tmp_calcs[col]["offset"] = offset
    #print(f"{col} offset: {offset} coef: {coef}")
    #df[f"calc_{col}"] = (df[col] - offset) * coef
    z = tmp_calcs[col]["poly_fit"]
    df[f"calc_{col}"] = z[0] + df[col] * z[1] + df[col]**2 * z[2]


import plotly.graph_objects as go
px.scatter(df, x="target", y=["calc_tmp1", "calc_tmp2"]).show()
#for col in ["tmp1", "tmp2"]:
#    x = np.array([2, 48])
#    y = [(tmp_calcs[col]["median"][tmp] - tmp_calcs[col]["offset"]) * tmp_calcs[col]["coef"] for tmp in x]
#    fig.add_trace(
#        go.Scatter(x=x, y=y, name="slope", line_shape='linear')
#    )
#
#fig.show()
